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


	// Check if jwt token was included
	std::string jwt;
	try {
		jwt = payload.at("headers").at("Authorization").get<std::string>();
	} catch (nlohmann::json::exception& e) {
		response["statusCode"] = 403;
		response["message"] = "Authorization header not found. Must supply Authorization header.";
		return invocation_response::success(response.dump(), "application/json");
	}

	if (!validate_token(jwt)) {
		response["statusCode"] = 403;
	} else {
		Aws::SDKOptions options;
		Aws::InitAPI(options);

		nlohmann::json body = payload.at("body");



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