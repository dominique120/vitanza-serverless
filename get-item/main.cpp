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

	Aws::SDKOptions options;

#ifdef LOGGING
	options.loggingOptions.logLevel = Aws::Utils::Logging::LogLevel::Trace;
	options.loggingOptions.logger_create_fn = GetConsoleLoggerFactory();
#endif // LOGGING


	// Check to see if jwt token was included
	std::string jwt;
	try {
		jwt = payload.at("headers").at("x-vts-auth").get<std::string>();
	} catch (nlohmann::json::exception& e) {
		response["statusCode"] = 403;
		response["message"] = "x-vts-auth header not found. Must supply \"x-vts-auth\" header with bearer token.";
		return invocation_response::success(response.dump(), "application/json");
	}
	std::string error;
	if (!validate_token(jwt, error)) {
		response["statusCode"] = 403;
		response["message"] = error;
	} else {
		Aws::SDKOptions options;
		Aws::InitAPI(options);

		nlohmann::json parameters = payload.at("queryStringParameters");

		// Create a Primary key(simple or composite)
		alddb::DynamoDB::PrimaryKey pk(parameters);

		// Run operation 
		nlohmann::json result;
		alddb::DynamoDB::get_item(ddbcli(), "Vitanza", pk, result);

		// Set response body and status code
		// If its empty then 204, else 200
		if (result.empty()) {
			response["statusCode"] = 204;
		} else {
			response["body"] = result.dump();
			response["statusCode"] = 200;
		}
	}

	return invocation_response::success(response.dump(), "application/json");
}


int main() {
	run_handler(my_handler);
	return 0;
}