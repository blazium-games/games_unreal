// The default player-session store must not leave the account bearer in a file
// another local user can read when the save directory is traversable.
//
// These checks fail while UBlaziumPlayerSession::Initialize's SaveSession writes
// that bearer through FFileHelper::SaveStringToFile. They pass when the default
// store stops doing that, or when a file that does hold it is owner-only.
//
// Build and run: tests/run_session_file_exposure.sh

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "../../Source/BlaziumGames/Private/BlaziumPlayerSession.cpp"

namespace {

int g_failures = 0;

void Expect(bool ok, const std::string& name) {
	if (ok) {
		std::cout << "PASS " << name << "\n";
		return;
	}
	std::cout << "FAIL " << name << "\n";
	++g_failures;
}

std::string ShellQuote(const std::string& text) {
	std::string out = "'";
	for (char c : text) {
		if (c == '\'') out += "'\\''";
		else out.push_back(c);
	}
	out.push_back('\'');
	return out;
}

std::string B64(const std::string& in) {
	static const char* alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	std::string out;
	int val = 0;
	int valb = -6;
	for (unsigned char c : in) {
		val = (val << 8) + c;
		valb += 8;
		while (valb >= 0) {
			out.push_back(alphabet[(val >> valb) & 0x3F]);
			valb -= 6;
		}
	}
	if (valb > -6) out.push_back(alphabet[((val << 8) >> (valb + 8)) & 0x3F]);
	while (out.size() % 4) out.push_back('=');
	return out;
}

// Obvious placeholder claims. ReadClaims does not check a signature.
std::string PlaceholderSession(const char* user, int64 exp, int64 iat) {
	const std::string header = "{\"alg\":\"none\",\"typ\":\"JWT\"}";
	const std::string payload = std::string("{\"user_uid\":\"") + user + "\",\"exp\":" + std::to_string(exp) + ",\"iat\":" + std::to_string(iat) + "}";
	return B64(header) + "." + B64(payload) + ".placeholder";
}

std::string Trim(std::string text) {
	while (!text.empty() && (text.back() == '\n' || text.back() == '\r' || text.back() == ' ' || text.back() == '\t')) text.pop_back();
	size_t i = 0;
	while (i < text.size() && (text[i] == '\n' || text[i] == '\r' || text[i] == ' ' || text[i] == '\t')) ++i;
	return text.substr(i);
}

bool OtherUserReads(const std::string& path, std::string& out) {
	const std::string dest = "/tmp/session-file-other-user-read.txt";
	const std::string cmd = "sudo -n -u nobody cat -- " + ShellQuote(path) + " > " + ShellQuote(dest) + " 2>/dev/null";
	const int rc = std::system(cmd.c_str());
	if (rc != 0) return false;
	FILE* file = std::fopen(dest.c_str(), "rb");
	if (!file) return false;
	std::string bytes;
	char buf[4096];
	while (true) {
		const size_t n = std::fread(buf, 1, sizeof buf, file);
		if (n == 0) break;
		bytes.append(buf, buf + n);
	}
	std::fclose(file);
	out = bytes;
	return true;
}

// True when a file this process created through the engine writer holds one of
// the session bearers and `nobody` can read it. That is the shared-machine case
// for a traversable save directory.
bool OtherUserCanReadBearer(const std::vector<std::string>& bearers, std::string& path_out) {
	for (const std::string& path : g_files_opened_for_write) {
		std::string got;
		if (!OtherUserReads(path, got)) continue;
		const std::string trimmed = Trim(got);
		for (const std::string& bearer : bearers) {
			if (!bearer.empty() && trimmed == bearer) {
				path_out = path;
				return true;
			}
		}
	}
	return false;
}

struct World {
	std::string saved;
	UBlaziumPlayerSession session;
	std::string first;
	std::string renewed;
	int64 now = 1500;

	explicit World(const std::string& saved_dir) : saved(saved_dir) {
		FPaths::SavedDir = saved;
		g_files_opened_for_write.clear();
		g_http_calls.clear();
		first = PlaceholderSession("player-placeholder", 10000, 1000);
		renewed = PlaceholderSession("player-placeholder", 17500, 7500);
		FSubsystemCollectionBase collection;
		session.Initialize(collection);
		session.Clock = [this]() { return now; };
		session.ApiBase = TEXT("http://api.test");
		session.LoginBase = TEXT("http://login.test");
		session.Configure(TEXT("game-placeholder"));
		g_http_handler = [this](const HttpCall& call) { return Reply(call); };
	}

	~World() {
		session.Deinitialize();
		g_http_handler = nullptr;
	}

	HttpReply Reply(const HttpCall& call) const {
		if (call.verb == "POST" && call.url == "http://login.test/api/v1/session") {
			return {200, "{\"cid\":\"cid-placeholder\",\"secret\":\"signin-secret-placeholder\",\"login_url\":\"https://login.example/placeholder\",\"user_code\":\"PLACEHOLDER\",\"verification_url\":\"https://login.example/connect\"}"};
		}
		if (call.verb == "GET" && call.url.find("http://login.test/api/v1/session/") == 0) {
			return {200, std::string("{\"state\":\"complete\",\"jwt\":\"") + first + "\",\"display_name\":\"Placeholder\"}"};
		}
		if (call.url == "http://api.test/public/token/refresh") {
			return {200, std::string("{\"token\":\"") + renewed + "\"}"};
		}
		if (call.url.find("/tokens") != std::string::npos) {
			return {200, "{\"data\":{\"token\":\"scoped-placeholder\",\"expires_in\":600}}"};
		}
		if (call.url == "http://api.test/private/irc/token") {
			return {200, "{\"username\":\"placeholder\",\"token\":\"chat-placeholder\",\"host\":\"irc.example\",\"port\":6697,\"tls\":true}"};
		}
		if (call.url == "http://api.test/private/account") {
			return {200, "{\"display_name\":\"Placeholder\",\"username\":\"placeholder\"}"};
		}
		if (call.url == "http://api.test/private/logout") return {200, "{}"};
		return {404, "{\"error\":\"unexpected\"}"};
	}
};

bool HasAuth(const std::string& url_part, const std::string& session, const std::string& body_part = std::string()) {
	const std::string want = std::string("Bearer ") + session;
	for (const HttpCall& call : g_http_calls) {
		if (call.url.find(url_part) == std::string::npos) continue;
		if (!body_part.empty() && call.body.find(body_part) == std::string::npos) continue;
		if (call.authorization == want) return true;
	}
	return false;
}

std::string MakeSavedDir() {
	char tmpl[] = "/tmp/blazium-saved-XXXXXX";
	char* dir = mkdtemp(tmpl);
	if (!dir) return {};
	// The finding's condition: the save directory is traversable by other users.
	// mkdtemp is owner-only, so open it the way a shared or project Saved dir is.
	if (chmod(dir, 0755) != 0) return {};
	return dir;
}

void ExpectBearerNotReadable(const std::vector<std::string>& bearers, const char* name) {
	std::string path;
	const bool readable = OtherUserCanReadBearer(bearers, path);
	if (readable) {
		struct stat st;
		std::string mode = "unknown";
		if (stat(path.c_str(), &st) == 0) {
			char buf[16];
			std::snprintf(buf, sizeof buf, "%o", static_cast<unsigned>(st.st_mode & 0777));
			mode = buf;
		}
		std::cout << "FAIL " << name << " (another user read the session bearer from " << path << " mode " << mode << ")\n";
		++g_failures;
		return;
	}
	std::cout << "PASS " << name << "\n";
}

void TestSignInRefreshTokensChatAndRestore() {
	const std::string saved = MakeSavedDir();
	Expect(!saved.empty(), "save directory exists");
	if (saved.empty()) return;
	umask(022);

	FString user;
	int64 exp = 0;
	int64 iat = 0;
	const std::string sample = PlaceholderSession("player-placeholder", 10000, 1000);
	Expect(FBlaziumSessionRules::ReadClaims(FString(sample.c_str()), user, exp, iat) && user == TEXT("player-placeholder"),
		"placeholder session has a readable user claim");

	{
		World world(saved);
		world.session.StartSignIn();
		Expect(world.session.IsSignedIn(), "sign-in adopts the session");
		Expect(world.session.GetState() == EBlaziumSignInState::Complete, "sign-in completes");
		Expect(HasAuth("/private/account", world.first), "account request sends the session as bearer");

		world.now = 7500;
		bool refreshed = false;
		world.session.RefreshIfDue([&]() { refreshed = true; });
		Expect(refreshed, "refresh callback runs");
		Expect(HasAuth("/public/token/refresh", world.first), "refresh sends the saved session as bearer");
		Expect(world.session.ExportSession() == FString(world.renewed.c_str()), "refresh adopts the new session");

		for (const char* audience : {"sapi", "lobby", "server"}) {
			bool ok = false;
			std::string code;
			world.session.GetToken(TEXT(audience), [&](bool got, const FString&, const FString& why) {
				ok = got;
				code = why.s;
			});
			Expect(ok, std::string("token request succeeds for ") + audience);
			const std::string body = std::string("{\"audience\":\"") + audience + "\"}";
			Expect(HasAuth("/tokens", world.renewed, body), std::string("token request sends the session as bearer for ") + audience);
		}

		bool chat_ok = false;
		world.session.GetChatAccess([&](const FBlaziumChatAccess& access) { chat_ok = access.bOk; });
		Expect(chat_ok, "chat access succeeds");
		Expect(HasAuth("/private/irc/token", world.renewed), "chat request sends the session as bearer");

		ExpectBearerNotReadable({world.first, world.renewed},
			"sign-in does not leave a bearer another user can read");
	}

	// The process has exited without SignOut. A later run restores whatever was saved.
	{
		FPaths::SavedDir = saved;
		g_http_calls.clear();
		UBlaziumPlayerSession again;
		FSubsystemCollectionBase collection;
		again.Initialize(collection);
		again.Clock = []() { return static_cast<int64>(1500); };
		again.ApiBase = TEXT("http://api.test");
		again.LoginBase = TEXT("http://login.test");
		again.Configure(TEXT("game-placeholder"));
		const std::string renewed = PlaceholderSession("player-placeholder", 17500, 7500);
		g_http_handler = [&](const HttpCall& call) {
			if (call.url == "http://api.test/private/account") return HttpReply{200, "{\"display_name\":\"Placeholder\",\"username\":\"placeholder\"}"};
			if (call.url.find("/tokens") != std::string::npos) return HttpReply{200, "{\"data\":{\"token\":\"scoped-placeholder\",\"expires_in\":600}}"};
			return HttpReply{404, "{}"};
		};
		again.RestoreSaved();
		// At 1500 the renewed session (iat 7500) is not yet due, so restore does not refresh.
		// If the file was the bearer, restore signs the player back in and the next token
		// request sends those file bytes. A fixed store may have nothing to restore; that
		// is fine. What must not happen is a traversable file of the bearer.
		if (again.IsSignedIn()) {
			bool ok = false;
			again.GetToken(TEXT("sapi"), [&](bool got, const FString&, const FString&) { ok = got; });
			Expect(ok, "restored session can ask for a sapi token");
			Expect(HasAuth("/tokens", again.ExportSession().s), "restored session is sent as bearer");
		}
		ExpectBearerNotReadable({sample, renewed, again.ExportSession().s},
			"restored save does not leave a bearer another user can read");
		again.Deinitialize();
		g_http_handler = nullptr;
	}

	std::system(("rm -rf -- " + ShellQuote(saved)).c_str());
}

void TestUseSession() {
	const std::string saved = MakeSavedDir();
	Expect(!saved.empty(), "use-session save directory exists");
	if (saved.empty()) return;
	umask(022);
	World world(saved);
	const std::string why = world.session.UseSession(FString(world.first.c_str())).s;
	Expect(why.empty(), "UseSession accepts the placeholder session");
	Expect(world.session.IsSignedIn(), "UseSession signs in");
	world.now = 7500;
	world.session.RefreshIfDue([]() {});
	Expect(HasAuth("/public/token/refresh", world.first), "UseSession value is the refresh bearer");
	ExpectBearerNotReadable({world.first, world.renewed},
		"UseSession does not leave a bearer another user can read");
	std::system(("rm -rf -- " + ShellQuote(saved)).c_str());
}

}  // namespace

int main() {
	TestSignInRefreshTokensChatAndRestore();
	TestUseSession();
	if (g_failures != 0) {
		std::cout << g_failures << " failed\n";
		return 1;
	}
	std::cout << "all session file checks passed\n";
	return 0;
}
