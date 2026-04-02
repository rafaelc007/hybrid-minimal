// ============================================================================
// Hybrid Minimal Watchface — PebbleKit JS Companion (Stub)
// ============================================================================
// Phase 1: Minimal stub. Will be expanded for weather + Clay settings.

Pebble.addEventListener('ready', function() {
  console.log('PebbleKit JS ready!');
});

Pebble.addEventListener('appmessage', function(e) {
  console.log('Received message from watch: ' + JSON.stringify(e.payload));
});
