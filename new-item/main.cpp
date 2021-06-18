#include "../common/common.h"

using namespace aws::lambda_runtime;

invocation_response my_handler(invocation_request const& request) {

	// test to see if the payload the function got is a well formatted json
	nlohmann::json payload;
	try {
		payload = nlohmann::json::parse(request.payload);
	} catch (nlohmann::json::exception& e) {
		return invocation_response::failure(e.what(), "Payload parsing failed");
	}

	nlohmann::json response;
	response["isBase64Encoded"] = false;
	response["headers"]["Content-Type"] = "application/json";
	response["headers"]["Access-Control-Allow-Origin"] = "http://localhost:4200";
	response["headers"]["Access-Control-Allow-Credentials"] = "true";
	response["headers"]["Access-Control-Allow-Methods"] = "GET, POST, PUT, DELETE, OPTIONS";
	response["headers"]["Access-Control-Max-Age"] = 8600;
	response["headers"]["Access-Control-Allow-Headers"] = "x-vts-auth";

	Aws::SDKOptions options;

#ifdef LOGGING
	options.loggingOptions.logLevel = Aws::Utils::Logging::LogLevel::Trace;
	options.loggingOptions.logger_create_fn = GetConsoleLoggerFactory();
#endif // LOGGING


	// Check to see if jwt token was included
	std::string jwt;
	try {
		jwt = payload.at("headers").at("x-vts-auth").get<std::string>();
	} catch (nlohmann::json::exception&) {
		response["statusCode"] = 403;
		response["message"] = "x-vts-auth header not found. Must supply \"x-vts-auth\" header with bearer token.";
		return invocation_response::success(response.dump(), "application/json");
	}
	std::string error;
	nlohmann::json body;
	if (!validate_token(jwt, error)) {
		response["statusCode"] = 403;
		response["message"] = error;
	} else {
		Aws::SDKOptions options;
		Aws::InitAPI(options);

		body = nlohmann::json::parse(payload.at("body").get<std::string>());

		if (alddb::DynamoDB::put_item(ddbcli(), body, "Vitanza")) {
			response["statusCode"] = 201;
		} else {
			response["body"] = "Bad request, failed to insert record";
			response["statusCode"] = 400;
		}
	}

	return invocation_response::success(response.dump(), "application/json");
}


int main() {
	run_handler(my_handler);
	return 0;
}