#pragma once

#include <algorithm>
#include <chrono>

enum class whip_patch_status { success, retry, failed };

struct whip_patch_result {
	whip_patch_status status = whip_patch_status::failed;
	std::chrono::milliseconds retry_after{0};
};

inline whip_patch_status whip_classify_patch(bool transport_ok, long response_code)
{
	if (!transport_ok || response_code == 408 || response_code == 429 ||
	    (response_code >= 500 && response_code < 600)) {
		return whip_patch_status::retry;
	}
	if (response_code >= 200 && response_code < 300) {
		return whip_patch_status::success;
	}
	return whip_patch_status::failed;
}

// Keep the same fragment until it is acknowledged. Wait must be interruptible.
// A longer Retry-After fails delivery instead of retrying before that deadline.
template<typename Send, typename Wait> bool whip_retry_patch(Send send, Wait wait)
{
	for (int attempt = 0; attempt < 3; attempt++) {
		const auto result = send();
		if (result.status == whip_patch_status::success) {
			return true;
		}
		if (result.status != whip_patch_status::retry || attempt == 2) {
			return false;
		}
		const auto delay = std::max(std::chrono::milliseconds(250 << attempt), result.retry_after);
		if (delay > std::chrono::seconds(30) || !wait(delay)) {
			return false;
		}
	}
	return false;
}
