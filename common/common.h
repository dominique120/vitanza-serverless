#include <dynamodb-cpp/dynamodb-cpp.h>
#include <aws/core/Aws.h>
#include <nlohmann/json.hpp>
#include <jwt-cpp/jwt.h>

#include <memory>

#include <aws/lambda-runtime/runtime.h>
#include <aws/core/platform/Environment.h>
#include <aws/core/client/ClientConfiguration.h>
#include <aws/core/auth/AWSCredentialsProvider.h>

#ifdef LOGGING
#include <aws/core/utils/logging/LogLevel.h>
#include <aws/core/utils/logging/ConsoleLogSystem.h>
#include <aws/core/utils/logging/LogMacros.h>
std::function<std::shared_ptr<Aws::Utils::Logging::LogSystemInterface>()> GetConsoleLoggerFactory() {
	return [] {
		return Aws::MakeShared<Aws::Utils::Logging::ConsoleLogSystem>(
			"console_logger", Aws::Utils::Logging::LogLevel::Trace);
	};
}
#endif // LOGGING

static char const TAG[] = "lambda";

// Generate DynamoDB for Lambda environment
inline std::unique_ptr<Aws::DynamoDB::DynamoDBClient> ddbcli() {
	Aws::Client::ClientConfiguration config;
	config.region = Aws::Environment::GetEnv("AWS_REGION");
	config.caFile = "/etc/pki/tls/certs/ca-bundle.crt";
	config.disableExpectHeader = true;

	auto credentialsProvider = Aws::MakeShared<Aws::Auth::EnvironmentAWSCredentialsProvider>(TAG);

	auto cli =
		std::make_unique<Aws::DynamoDB::DynamoDBClient>(credentialsProvider, config);
	return cli;
}

// Same function as Vitanza service but removed references to g_config for SHA512_SECRET and JWT_ISSUER
// Issuer is also changed
inline bool validate_token(const std::string& token_header, std::string& error) {
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

	} catch (const jwt::token_verification_exception& e) {
		error = e.what();
		return false;
	} catch (const nlohmann::json::exception& e) {
		error = e.what();
		return false;
	} catch (const std::invalid_argument& e) {
		error = e.what();
		return false;
	} catch (const std::runtime_error& e) {
		error = e.what();
		return false;
	}
}