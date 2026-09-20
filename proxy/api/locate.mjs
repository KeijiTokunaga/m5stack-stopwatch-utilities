import { authorize, buildStaticMapUrl, locate, validateAccessPoints } from '../lib/google.mjs'

export default async function handler(request, response) {
  response.setHeader('Cache-Control', 'no-store')
  if (request.method !== 'POST') return response.status(405).json({ error: 'method_not_allowed' })
  if (!authorize(request.headers.authorization, process.env.DEVICE_TOKEN)) {
    return response.status(401).json({ error: 'unauthorized' })
  }
  let accessPoints
  try {
    accessPoints = validateAccessPoints(request.body?.wifiAccessPoints)
  } catch {
    return response.status(400).json({ error: 'invalid_access_points' })
  }
  try {
    const result = await locate(accessPoints, process.env.GOOGLE_GEOLOCATION_API_KEY)
    const mapUrl = buildStaticMapUrl({
      ...result,
      apiKey: process.env.GOOGLE_STATIC_MAPS_API_KEY,
      signingSecret: process.env.GOOGLE_MAPS_URL_SIGNING_SECRET,
    })
    return response.status(200).json({ ...result, mapUrl })
  } catch (error) {
    if (error.status === 404) return response.status(404).json({ error: 'location_unavailable' })
    return response.status(502).json({ error: 'upstream_failure' })
  }
}
