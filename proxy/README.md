# Location proxy

This Vercel Function keeps Google signing credentials off the StopWatch. The device sends an authenticated list of nearby BSSIDs, signal strengths and channels to `/api/locate`; the same response includes the resulting coordinates, accuracy and a signed Google Static Maps URL tied to that fix. The device downloads the image directly from Google so the proxy does not re-host Maps content. The endpoint is not cached.

## Deploy

1. Create a Google Cloud project with billing enabled, then enable **Geolocation API** and **Maps Static API**.
2. Create separate keys: a server-only key restricted to **Geolocation API**, and another key restricted to **Maps Static API**. Create a Maps Static API URL-signing secret. Set conservative daily quotas and billing alerts; the signed Static Maps URL necessarily contains its API key, so separating the keys, API restrictions and signing are mandatory.
3. Generate a long random device token, for example `openssl rand -hex 32`.
4. Import this `proxy/` directory as a Vercel project and set the four variables shown in `.env.example` for Production. Do not prefix them with `NEXT_PUBLIC_` and do not commit their values.
5. Deploy, then enter the resulting HTTPS origin and device token in the StopWatch Wi-Fi setup page and explicitly enable consent.

For local tests, run `npm test`. Rotate `DEVICE_TOKEN` if the device or token may have been exposed. The proxy deliberately does not log request bodies or locations; keep platform request logging and third-party drains disabled for these endpoints.

Google billing, API-key restrictions, attribution, caching and privacy requirements are summarized in [the research note](../docs/google-wifi-location-research.md).
