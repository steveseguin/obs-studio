#include "../../../plugins/obs-webrtc/whip-link-utils.h"

#include <iostream>
#include <stdexcept>

static void require(bool condition, const char *message)
{
	if (!condition) {
		throw std::runtime_error(message);
	}
}

int main()
{
	for (const char *credential : {"ordinary", "pass,word", "pass;word", "pass,;word"}) {
		const std::string header = "<turn:turn.example>; rel=\"ice-server\"; username=\"user\"; credential=\"";
		auto links = whip_parse_link_header(header + credential + "\"");
		require(links.size() == 1, "Quoted credential split the link");
		require(links[0].parameters["credential"] == credential, "Quoted credential was truncated");
	}
	auto links = whip_parse_link_header(
		"<turn:turn.example?value=a,b;c>; ReL = ice-server; USERNAME = user; CREDENTIAL = password, "
		"<stun:stun.example>; rel=\"ice-server\"");
	require(links.size() == 2, "Multiple links or URI delimiters were not preserved");
	require(links[0].url == "turn:turn.example?value=a,b;c", "URI was truncated");
	require(links[0].parameters["rel"] == "ice-server", "Parameter names were not case-normalized");
	require(links[0].parameters["username"] == "user", "Unquoted username lost");
	require(links[0].parameters["credential"] == "password", "Unquoted password lost");
	links = whip_parse_link_header(R"(<turn:turn.example>; credential="pa\\ss\"word"; username="")");
	require(links.size() == 1 && links[0].parameters["credential"] == "pa\\ss\"word", "Quoted-pair escapes lost");
	require(links[0].parameters["username"].empty(), "Empty quoted value changed");
	links = whip_parse_link_header("<turn:turn.example>; rel=ice-server; rel=other");
	require(links.size() == 1 && links[0].parameters["rel"] == "ice-server", "Duplicate rel replaced first value");
	for (const char *invalid : {"<turn:turn.example>; credential=\"unfinished", "<turn:turn.example>; credential=\"a\"tail",
				   "<turn:turn.example>; credential=bad value",
				   "<turn:turn.example>; credential=\"a\r\nb\""}) {
		require(whip_parse_link_header(invalid).empty(), "Malformed credential was accepted");
	}
	for (int c = 32; c < 127; c++) {
		std::string quoted = "\"prefix";
		if (c == '"' || c == '\\') {
			quoted += '\\';
		}
		quoted += char(c);
		quoted += "suffix\"";
		links = whip_parse_link_header("<turn:turn.example>; credential=" + quoted);
		require(links.size() == 1 && links[0].parameters["credential"] == "prefix" + std::string(1, char(c)) + "suffix",
			"Printable quoted character failed to round-trip");
	}
	std::cout << "Link parameters: tokens, quoted separators, escapes, casing, duplicates and invalid input passed\n";
}
