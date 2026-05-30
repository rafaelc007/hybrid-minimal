// ============================================================================
// Hybrid Minimal Watchface — PebbleKit JS Companion
// ============================================================================

// Widget IDs — must mirror src/c/widgets.h
var WIDGETS = ['Steps', 'Battery', 'Time', 'Date', 'Weather'];
var DEFAULT_ORDER = [0, 1, 2, 3, 4];

// ----------------------------------------------------------------------------
// Weather (Open-Meteo)
// ----------------------------------------------------------------------------
function wmoToken(code) {
  if (code === 0)  return 'clear';
  if (code <= 3)   return 'cloud';
  if (code <= 48)  return 'cloud';
  if (code <= 55)  return 'drizzle';
  if (code <= 57)  return 'sleet';
  if (code <= 65)  return 'rain';
  if (code <= 67)  return 'sleet';
  if (code <= 77)  return 'snow';
  if (code <= 82)  return 'rain';
  if (code <= 86)  return 'snow';
  if (code <= 99)  return 'thunder';
  return 'cloud';
}

function fetchWeather() {
  navigator.geolocation.getCurrentPosition(
    function(pos) {
      var lat = pos.coords.latitude.toFixed(4);
      var lon = pos.coords.longitude.toFixed(4);
      var url = 'https://api.open-meteo.com/v1/forecast' +
                '?latitude=' + lat +
                '&longitude=' + lon +
                '&current_weather=true' +
                '&temperature_unit=celsius';
      var xhr = new XMLHttpRequest();
      xhr.onload = function() {
        try {
          var data = JSON.parse(this.responseText);
          var cw   = data.current_weather;
          var temp = Math.round(cw.temperature);
          var cond = wmoToken(cw.weathercode);
          Pebble.sendAppMessage(
            { WeatherTemp: temp, WeatherIcon: cond },
            function() { console.log('Weather sent: ' + temp + '\u00b0 ' + cond); },
            function(e) { console.log('Weather send failed: ' + JSON.stringify(e)); }
          );
        } catch (e) {
          console.log('Weather parse error: ' + e);
        }
      };
      xhr.open('GET', url);
      xhr.send();
    },
    function(err) {
      console.log('Geolocation error: ' + err.message);
    },
    { timeout: 15000 }
  );
}

// ----------------------------------------------------------------------------
// Settings page
// ----------------------------------------------------------------------------
function getSavedOrder() {
  var raw = localStorage.getItem('slotOrder');
  if (!raw) return DEFAULT_ORDER.slice();
  try {
    var arr = JSON.parse(raw);
    if (Array.isArray(arr) && arr.length === WIDGETS.length) return arr;
  } catch (e) { /* fallthrough */ }
  return DEFAULT_ORDER.slice();
}

function buildConfigHtml(order) {
  var labelsJson = JSON.stringify(WIDGETS);
  var orderJson  = JSON.stringify(order);
  return '<!doctype html>' +
'<html><head><meta charset="utf-8">' +
'<meta name="viewport" content="width=device-width,initial-scale=1">' +
'<title>Hybrid Minimal</title>' +
'<style>' +
'body{font-family:-apple-system,system-ui,sans-serif;background:#222;color:#eee;margin:0;padding:16px}' +
'h1{font-size:18px;margin:0 0 4px}' +
'p{color:#aaa;font-size:13px;margin:0 0 16px}' +
'.zone{margin:18px 0 6px;color:#888;text-transform:uppercase;font-size:11px;letter-spacing:1px}' +
'ul{list-style:none;padding:0;margin:0}' +
'li{display:flex;align-items:center;background:#333;border-radius:6px;padding:10px 12px;margin-bottom:6px}' +
'.name{flex:1;font-size:15px}' +
'button{background:#555;color:#fff;border:0;border-radius:4px;width:32px;height:32px;font-size:18px;margin-left:4px}' +
'button:disabled{opacity:.3}' +
'.save{display:block;width:100%;background:#0a84ff;border:0;color:#fff;padding:14px;font-size:16px;border-radius:8px;margin-top:24px}' +
'</style></head><body>' +
'<h1>Widget Order</h1>' +
'<p>The first two appear in the outer ring; the next three in the inner stack.</p>' +
'<div class="zone">Outer ring (top \u2192 bottom)</div>' +
'<ul id="outer"></ul>' +
'<div class="zone">Inner stack (top \u2192 bottom)</div>' +
'<ul id="inner"></ul>' +
'<button class="save" id="save">Save</button>' +
'<script>' +
'var labels=' + labelsJson + ';' +
'var order=' + orderJson + ';' +
'function render(){' +
'  var outer=document.getElementById("outer");' +
'  var inner=document.getElementById("inner");' +
'  outer.innerHTML="";inner.innerHTML="";' +
'  order.forEach(function(id,idx){' +
'    var li=document.createElement("li");' +
'    var name=document.createElement("span");' +
'    name.className="name";name.textContent=labels[id];' +
'    var up=document.createElement("button");up.textContent="\u2191";' +
'    up.disabled=(idx===0);' +
'    up.onclick=function(){var t=order[idx-1];order[idx-1]=order[idx];order[idx]=t;render();};' +
'    var dn=document.createElement("button");dn.textContent="\u2193";' +
'    dn.disabled=(idx===order.length-1);' +
'    dn.onclick=function(){var t=order[idx+1];order[idx+1]=order[idx];order[idx]=t;render();};' +
'    li.appendChild(name);li.appendChild(up);li.appendChild(dn);' +
'    (idx<2?outer:inner).appendChild(li);' +
'  });' +
'}' +
'render();' +
'document.getElementById("save").onclick=function(){' +
'  var payload=encodeURIComponent(JSON.stringify({order:order}));' +
'  document.location="pebblejs://close#"+payload;' +
'};' +
'</script></body></html>';
}

Pebble.addEventListener('showConfiguration', function() {
  var order = getSavedOrder();
  var html = buildConfigHtml(order);
  Pebble.openURL('data:text/html;charset=utf-8,' + encodeURIComponent(html));
});

Pebble.addEventListener('webviewclosed', function(e) {
  if (!e || !e.response) return;
  try {
    var cfg = JSON.parse(decodeURIComponent(e.response));
    if (cfg && Array.isArray(cfg.order) && cfg.order.length === WIDGETS.length) {
      localStorage.setItem('slotOrder', JSON.stringify(cfg.order));
      Pebble.sendAppMessage(
        { SlotOrder: cfg.order },
        function() { console.log('SlotOrder sent: ' + cfg.order.join(',')); },
        function(err) { console.log('SlotOrder send failed: ' + JSON.stringify(err)); }
      );
    }
  } catch (e) {
    console.log('Config parse error: ' + e);
  }
});

Pebble.addEventListener('ready', function() {
  console.log('PebbleKit JS ready');
  fetchWeather();
});

Pebble.addEventListener('appmessage', function(e) {
  if (e && e.payload && e.payload.RequestWeather) {
    fetchWeather();
  }
});
