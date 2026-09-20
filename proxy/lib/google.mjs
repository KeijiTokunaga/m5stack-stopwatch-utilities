import { createHmac, timingSafeEqual } from 'node:crypto'

const MAC_PATTERN = /^(?:[0-9a-f]{2}:){5}[0-9a-f]{2}$/i

export function authorize(header, expectedToken) {
  if (!expectedToken || typeof header !== 'string' || !header.startsWith('Bearer ')) return false
  const supplied = Buffer.from(header.slice(7))
  const expected = Buffer.from(expectedToken)
  return supplied.length === expected.length && timingSafeEqual(supplied, expected)
}

function usableMac(macAddress) {
  if (!MAC_PATTERN.test(macAddress)) return false
  const normalized = macAddress.toLowerCase()
  if (normalized === 'ff:ff:ff:ff:ff:ff') return false
  if (normalized.startsWith('00:00:5e:')) return false
  const first = Number.parseInt(normalized.slice(0, 2), 16)
  return (first & 0x03) === 0
}

export function validateAccessPoints(input) {
  if (!Array.isArray(input)) throw new TypeError('At least two usable access points are required')
  const unique = new Map()
  for (const candidate of input) {
    const macAddress = String(candidate?.macAddress ?? '').toLowerCase()
    const signalStrength = Number(candidate?.signalStrength)
    const channel = Number(candidate?.channel)
    if (!usableMac(macAddress) || !Number.isFinite(signalStrength) || signalStrength < -128 || signalStrength > -10 ||
        !Number.isInteger(channel) || channel < 1 || channel > 196) continue
    const current = unique.get(macAddress)
    if (!current || signalStrength > current.signalStrength) {
      unique.set(macAddress, { macAddress, signalStrength, channel })
    }
  }
  const accessPoints = [...unique.values()].sort((a, b) => b.signalStrength - a.signalStrength).slice(0, 20)
  if (accessPoints.length < 2) throw new TypeError('At least two usable access points are required')
  return accessPoints
}

export function buildStaticMapUrl({ lat, lng, apiKey, signingSecret }) {
  if (!Number.isFinite(lat) || lat < -90 || lat > 90 || !Number.isFinite(lng) || lng < -180 || lng > 180) {
    throw new TypeError('Invalid coordinates')
  }
  if (!apiKey || !signingSecret) throw new TypeError('Missing Google Maps credentials')
  const coordinate = `${lat.toFixed(6)},${lng.toFixed(6)}`
  const path = '/maps/api/staticmap'
  const params = new URLSearchParams({
    center: coordinate,
    zoom: '15',
    size: '320x320',
    format: 'jpg-baseline',
    maptype: 'roadmap',
    markers: `color:red|label:P|${coordinate}`,
    key: apiKey,
  })
  const unsigned = `${path}?${params}`
  const secret = Buffer.from(signingSecret.replace(/-/g, '+').replace(/_/g, '/'), 'base64')
  const signature = createHmac('sha1', secret).update(unsigned).digest('base64url')
  return `https://maps.googleapis.com${unsigned}&signature=${signature}`
}

export async function locate(accessPoints, apiKey, fetchImpl = fetch) {
  if (!apiKey) throw new Error('Missing Google Maps API key')
  const response = await fetchImpl(`https://www.googleapis.com/geolocation/v1/geolocate?key=${encodeURIComponent(apiKey)}`, {
    method: 'POST',
    headers: { 'content-type': 'application/json' },
    body: JSON.stringify({ considerIp: false, wifiAccessPoints: validateAccessPoints(accessPoints) }),
  })
  const body = await response.json().catch(() => ({}))
  if (!response.ok) {
    const error = new Error(response.status === 404 ? 'Location unavailable' : 'Google Geolocation request failed')
    error.status = response.status
    throw error
  }
  const lat = Number(body?.location?.lat)
  const lng = Number(body?.location?.lng)
  const accuracy = Number(body?.accuracy)
  if (!Number.isFinite(lat) || lat < -90 || lat > 90 || !Number.isFinite(lng) || lng < -180 || lng > 180 ||
      !Number.isFinite(accuracy) || accuracy < 0) throw new Error('Invalid Google Geolocation response')
  return { lat, lng, accuracy }
}
