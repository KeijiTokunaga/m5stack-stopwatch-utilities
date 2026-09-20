import test from 'node:test'
import assert from 'node:assert/strict'
import {
  authorize,
  buildStaticMapUrl,
  validateAccessPoints,
} from '../lib/google.mjs'

test('authorization requires the exact bearer token', () => {
  assert.equal(authorize('Bearer device-secret', 'device-secret'), true)
  assert.equal(authorize('Bearer device-secrex', 'device-secret'), false)
  assert.equal(authorize(undefined, 'device-secret'), false)
  assert.equal(authorize('Bearer device-secret', ''), false)
})

test('access point validation keeps only safe unique physical BSSIDs', () => {
  const result = validateAccessPoints([
    { macAddress: '00:11:22:33:44:55', signalStrength: -42, channel: 6 },
    { macAddress: '00:11:22:33:44:55', signalStrength: -70, channel: 6 },
    { macAddress: '02:11:22:33:44:55', signalStrength: -50, channel: 1 },
    { macAddress: 'ff:ff:ff:ff:ff:ff', signalStrength: -60, channel: 11 },
    { macAddress: '00:25:96:ff:fe:12', signalStrength: -65, channel: 36 },
  ])
  assert.deepEqual(result, [
    { macAddress: '00:11:22:33:44:55', signalStrength: -42, channel: 6 },
    { macAddress: '00:25:96:ff:fe:12', signalStrength: -65, channel: 36 },
  ])
})

test('access point validation rejects unusable inputs and caps the request', () => {
  assert.throws(() => validateAccessPoints([{ macAddress: 'bad', signalStrength: -40, channel: 1 }]), /two usable/i)
  const many = Array.from({ length: 30 }, (_, index) => ({
    macAddress: `00:11:22:33:44:${index.toString(16).padStart(2, '0')}`,
    signalStrength: -20 - index,
    channel: 1 + index % 11,
  }))
  assert.equal(validateAccessPoints(many).length, 20)
})

test('static map URL is signed and preserves the complete 320px map', () => {
  const url = buildStaticMapUrl({
    lat: 35.681236,
    lng: 139.767125,
    apiKey: 'maps-key',
    signingSecret: 'a2V5',
  })
  assert.match(url, /^https:\/\/maps\.googleapis\.com\/maps\/api\/staticmap\?/)
  assert.match(url, /size=320x320/)
  assert.match(url, /format=jpg-baseline/)
  assert.match(url, /markers=color%3Ared%7Clabel%3AP%7C35\.681236%2C139\.767125/)
  assert.match(url, /key=maps-key/)
  assert.match(url, /signature=[A-Za-z0-9_-]+$/)
})
