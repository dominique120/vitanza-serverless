#include <dynamodb-cpp/dynamodb-cpp.h>
#include <aws/core/Aws.h>
#include "nlohmann/json.hpp"

#include <jwt-cpp/jwt.h>

#include <memory>

#include <aws/lambda-runtime/runtime.h>
#include <aws/core/platform/Environment.h>
#include <aws/core/client/ClientConfiguration.h>
#include <aws/core/auth/AWSCredentialsProvider.h>
#include <aws/core/utils/logging/LogLevel.h>
#include <aws/core/utils/logging/ConsoleLogSystem.h>
#include <aws/core/utils/logging/LogMacros.h>

using namespace aws::lambda_runtime;

static char const TAG[] = "lambda";

std::unique_ptr<Aws::DynamoDB::DynamoDBClient> ddbcli() {
	Aws::Client::ClientConfiguration config;
	config.region = Aws::Environment::GetEnv("AWS_REGION");
	config.caFile = "/etc/pki/tls/certs/ca-bundle.crt";
	config.disableExpectHeader = true;

	auto credentialsProvider = Aws::MakeShared<Aws::Auth::EnvironmentAWSCredentialsProvider>(TAG);

	auto cli =
		std::make_unique<Aws::DynamoDB::DynamoDBClient>(credentialsProvider, config);
	return cli;
}

// Disabled for normal usage
/*
std::function<std::shared_ptr<Aws::Utils::Logging::LogSystemInterface>()> GetConsoleLoggerFactory() {
	return [] {
		return Aws::MakeShared<Aws::Utils::Logging::ConsoleLogSystem>(
			"console_logger", Aws::Utils::Logging::LogLevel::Trace);
	};
}
*/

// Same function as Vitanza service but removed references to g_config for SHA512_SECRET and JWT_ISSUER
// Issuer is also changed
bool validate_token(const std::string& token_header) {
	try {
		std::string token = token_header;

		// Remove the "Bearer " section of the header
		token.erase(0, 7);

		const auto decoded = jwt::decode(token);

		const nlohmann::json jwt_payload = nlohmann::json::parse(decoded.get_payload());

		const jwt::claim user(jwt_payload.at("username").get<std::string>());
		const jwt::claim pwd(jwt_payload.at("password").get<std::string>());

		const auto verifier = jwt::verify()
			.allow_algorithm(jwt::algorithm::hs512{ "9E590B6B427FB984C1CC5F5E0373BB163A3DD9D3C371CAEF3726D68CB3B894341E3758430154A878DE6D2E639A54B2C783365BC4219EAB2B69D490D0793D967A" })
			.with_issuer("vts-serverless")
			.with_claim("username", user)
			.with_claim("password", pwd);

		verifier.verify(decoded);
		return true;

	} catch (jwt::token_verification_exception&) {
		return false;
	} catch (nlohmann::json::exception&) {
		return false;
	} catch (std::invalid_argument&) {
		return false;
	} catch (std::runtime_error&) {
		return false;
	}
}

invocation_response my_handler(invocation_request const& request) {

	// test to see if the payload the function got is a well formatted json
	nlohmann::json payload;
	try {
		payload = nlohmann::json::parse(request.payload);
	} catch (nlohmann::json::exception& e) {
		return invocation_response::failure(e.what(), "Payload parsing failed");
	}


	// Setup base response(to Api Gateway) parameters
	// Api Gateway will decode this and produce the http response
	nlohmann::json response;
	response["isBase64Encoded"] = false;
	response["headers"]["Content-Type"] = "application/json";

	// Check to see if jwt token was included
	std::string jwt;
	try {
		jwt = payload.at("headers").at("x-vts-auth").get<std::string>();
	} catch (nlohmann::json::exception& e) {
		response["statusCode"] = 403;
		response["message"] = "x-vts-auth header not found. Must supply x-vts-auth header with bearer token.";
		return invocation_response::success(response.dump(), "application/json");
	}


	if (!validate_token(jwt)) {
		response["statusCode"] = 403;
	} else {
		Aws::SDKOptions options;
		// Disable logging for normal usage
		// options.loggingOptions.logLevel = Aws::Utils::Logging::LogLevel::Trace;
		// options.loggingOptions.logger_create_fn = GetConsoleLoggerFactory();
		Aws::InitAPI(options);


		// Parse url parameters to extract keys
		nlohmann::json parameters = payload.at("queryStringParameters");

		// Parse body to get request body
		//nlohmann::json body = payload.at("body");


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
