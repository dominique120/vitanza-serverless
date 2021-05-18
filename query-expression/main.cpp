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

		/*
		* general query request body structure:
		*
		* "body" : {
		*		"expression" : "xx_expression_xx",
		*		"key_name"  : "xxxx",
		*		"expression_values" : {
		*						"value1" : "xxxx",
		*						"value2" : "xxxx",
		*						"value3" : "xxxx"
		*						}
		*			}
		*/

		body = nlohmann::json::parse(payload.at("body").get<std::string>());

		// get data for our query function
		std::string expression = body.at("expression").get<std::string>();
		std::string keyname = body.at("key_name").get<std::string>();
		nlohmann::json expression_values = body.at("expression_values");

		// create result and run query
		nlohmann::json result;
		alddb::DynamoDB::query_with_expression(ddbcli(), "Vitanza", keyname.c_str(), expression.c_str(), expression_values, result);
		
		// create response based on query result
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