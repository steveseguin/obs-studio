#include "../../../plugins/obs-webrtc/whip-trickle-utils.h"

#include <iostream>
#include <stdexcept>
#include <vector>

static void require(bool condition, const char *message)
{
	if (!condition) {
		throw std::runtime_error(message);
	}
}

int main()
{
	for (long code : {408, 429, 500, 502, 503, 504, 599}) {
		require(whip_classify_patch(true, code) == whip_patch_status::retry, "Transient failure not retried");
	}
	for (long code : {200, 201, 204}) {
		require(whip_classify_patch(true, code) == whip_patch_status::success, "Success not acknowledged");
	}
	for (long code : {400, 401, 403, 404, 412, 422, 428}) {
		require(whip_classify_patch(true, code) == whip_patch_status::failed, "Permanent failure retried");
	}
	require(whip_classify_patch(false, 0) == whip_patch_status::retry, "Transport failure not retried");

	int attempts = 0;
	std::vector<std::chrono::milliseconds> waits;
	auto wait = [&](auto delay) {
		waits.push_back(delay);
		return true;
	};
	bool success = whip_retry_patch(
		[&] {
			return whip_patch_result{++attempts < 3 ? whip_patch_status::retry
								: whip_patch_status::success};
		},
		wait);
	require(success && attempts == 3, "Transient failures were not retried until acknowledged");
	require(waits == std::vector{std::chrono::milliseconds(250), std::chrono::milliseconds(500)},
		"Exponential backoff changed");

	attempts = 0;
	waits.clear();
	success = whip_retry_patch(
		[&] {
			attempts++;
			return whip_patch_result{whip_patch_status::retry};
		},
		wait);
	require(!success && attempts == 3 && waits.size() == 2, "Retry exhaustion was not bounded");
	attempts = 0;
	waits.clear();
	success = whip_retry_patch(
		[&] {
			attempts++;
			return whip_patch_result{whip_patch_status::failed};
		},
		wait);
	require(!success && attempts == 1 && waits.empty(), "Permanent failure delayed the caller");
	attempts = 0;
	success = whip_retry_patch(
		[&] {
			attempts++;
			return whip_patch_result{whip_patch_status::retry};
		},
		[](auto) { return false; });
	require(!success && attempts == 1, "Cancelled wait sent another request");
	waits.clear();
	attempts = 0;
	success = whip_retry_patch(
		[&] {
			return whip_patch_result{++attempts == 1 ? whip_patch_status::retry
								 : whip_patch_status::success,
						 std::chrono::seconds(3)};
		},
		wait);
	require(success && waits == std::vector{std::chrono::milliseconds(3000)}, "Retry-After was ignored");
	waits.clear();
	success = whip_retry_patch([] { return whip_patch_result{whip_patch_status::retry, std::chrono::seconds(31)}; },
				   wait);
	require(!success && waits.empty(), "Long Retry-After was retried prematurely");
	std::cout
		<< "Trickle retry: acknowledgement, transient/permanent errors, backoff, limits and cancellation passed\n";
}
