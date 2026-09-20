# Privacy Policy — StopWatch Utilities

Effective: 2026-09-21

The Location feature is disabled by default. If you explicitly enable it, the device sends nearby Wi-Fi access-point BSSIDs, signal strengths and channels to the proxy URL you configure. The proxy sends those values to Google Maps Platform to obtain an approximate latitude, longitude and accuracy, then creates a signed Google Static Maps URL. Network providers receive the IP addresses needed to deliver these requests. SSIDs and Wi-Fi passwords are not sent to the proxy or Google.

The firmware keeps the returned location and map image only in RAM and does not persist them. This project's proxy code does not log request bodies or coordinates; the operator must also disable any request-body logging or third-party log drains. Google may process data under the [Google Privacy Policy](https://policies.google.com/privacy).

You can withdraw consent in Wi-Fi Settings. Saving with consent disabled stops future location requests and deletes the saved proxy URL and device token. Operators are responsible for securing and rotating tokens, setting API quotas, and publishing contact information appropriate to their deployment.
