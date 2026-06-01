// ============================================================================
// Hybrid Minimal Watchface — PebbleKit JS Companion
// ============================================================================

// Widget IDs — must mirror src/c/widgets.h
var WIDGETS = ['Steps', 'Battery', 'Time', 'Date', 'Weather'];
// Default visual order, top → bottom on the watchface.
var DEFAULT_ORDER = [0, 2, 3, 4, 1];

// Progress band color palette — values are Pebble GColor8 ARGB8 bytes.
var COLORS = [
  { name: 'Jazzberry', css: '#AA0055', argb: 0xE1 },
  { name: 'Red',       css: '#FF0000', argb: 0xF0 },
  { name: 'Orange',    css: '#FF5500', argb: 0xF4 },
  { name: 'Yellow',    css: '#FFFF00', argb: 0xFC },
  { name: 'Green',     css: '#00FF00', argb: 0xCC },
  { name: 'Cyan',      css: '#00FFFF', argb: 0xCF },
  { name: 'Blue',      css: '#0000FF', argb: 0xC3 },
  { name: 'Picton',    css: '#00AAFF', argb: 0xCB },
  { name: 'Magenta',   css: '#FF00FF', argb: 0xF3 },
  { name: 'White',     css: '#FFFFFF', argb: 0xFF },
  { name: 'Gray',      css: '#AAAAAA', argb: 0xEA }
];
var DEFAULT_COLOR_ARGB = 0xE1; // Jazzberry

// Progress bar shape: 0 = rounded square (default), 1 = sharp square.
var DEFAULT_BAR_STYLE = 0;

// Temperature unit: 'C' (celsius, default) or 'F' (fahrenheit).
var DEFAULT_UNITS = 'C';

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
  var units = getSavedUnits();
  var apiUnit = (units === 'F') ? 'fahrenheit' : 'celsius';
  navigator.geolocation.getCurrentPosition(
    function(pos) {
      var lat = pos.coords.latitude.toFixed(4);
      var lon = pos.coords.longitude.toFixed(4);
      var url = 'https://api.open-meteo.com/v1/forecast' +
                '?latitude=' + lat +
                '&longitude=' + lon +
                '&current_weather=true' +
                '&temperature_unit=' + apiUnit;
      var xhr = new XMLHttpRequest();
      xhr.onload = function() {
        try {
          var data = JSON.parse(this.responseText);
          var cw   = data.current_weather;
          var temp = Math.round(cw.temperature);
          var cond = wmoToken(cw.weathercode);
          Pebble.sendAppMessage(
            { WeatherTemp: temp, WeatherIcon: cond, WeatherUnits: units },
            function() { console.log('Weather sent: ' + temp + '\u00b0 ' + units + ' ' + cond); },
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
  var raw = localStorage.getItem('slotOrderV2');
  if (!raw) return DEFAULT_ORDER.slice();
  try {
    var arr = JSON.parse(raw);
    if (Array.isArray(arr) && arr.length === WIDGETS.length) return arr;
  } catch (e) { /* fallthrough */ }
  return DEFAULT_ORDER.slice();
}

function getSavedColor() {
  var raw = localStorage.getItem('progressColor');
  if (raw === null) return DEFAULT_COLOR_ARGB;
  var n = parseInt(raw, 10);
  return isNaN(n) ? DEFAULT_COLOR_ARGB : (n & 0xFF);
}

function getSavedBarStyle() {
  var raw = localStorage.getItem('barStyle');
  if (raw === null) return DEFAULT_BAR_STYLE;
  var n = parseInt(raw, 10);
  return (n === 1) ? 1 : 0;
}

function getSavedUnits() {
  var raw = localStorage.getItem('weatherUnits');
  return (raw === 'F') ? 'F' : 'C';
}

function buildConfigHtml(order) {
  var labelsJson = JSON.stringify(WIDGETS);
  var orderJson  = JSON.stringify(order);
  var colorsJson = JSON.stringify(COLORS);
  var selectedColor = getSavedColor();
  var selectedStyle = getSavedBarStyle();
  var selectedUnits = getSavedUnits();
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
'.swatches{display:flex;flex-wrap:wrap;gap:8px}' +
'.sw{width:40px;height:40px;border-radius:50%;border:3px solid #222;cursor:pointer;box-sizing:border-box}' +
'.sw.sel{border-color:#fff}' +
'.styles{display:flex;gap:8px}' +
'.style{flex:1;background:#333;color:#eee;border:2px solid #333;border-radius:6px;padding:12px;text-align:center;font-size:14px;cursor:pointer}' +
'.style.sel{border-color:#0a84ff;background:#1c3a5c}' +
'.save{display:block;width:100%;background:#0a84ff;border:0;color:#fff;padding:14px;font-size:16px;border-radius:8px;margin-top:24px}' +
'</style></head><body>' +
'<h1>Widget Order</h1>' +
'<p>Drag the arrows to reorder how widgets appear on the watchface.</p>' +
'<div class="zone">Widgets (top \u2192 bottom)</div>' +
'<ul id="stack"></ul>' +
'<div class="zone">Progress bar color</div>' +
'<div class="swatches" id="sw"></div>' +
'<div class="zone">Progress bar shape</div>' +
'<div class="styles">' +
'<div class="style" id="st0" data-v="0">Rounded</div>' +
'<div class="style" id="st1" data-v="1">Square</div>' +
'</div>' +
'<div class="zone">Temperature units</div>' +
'<div class="styles">' +
'<div class="style" id="unC" data-v="C">Celsius (\u00b0C)</div>' +
'<div class="style" id="unF" data-v="F">Fahrenheit (\u00b0F)</div>' +
'</div>' +
'<button class="save" id="save">Save</button>' +
'<script>' +
'var labels=' + labelsJson + ';' +
'var order=' + orderJson + ';' +
'var colors=' + colorsJson + ';' +
'var selColor=' + selectedColor + ';' +
'var selStyle=' + selectedStyle + ';' +
'var selUnits=' + JSON.stringify(selectedUnits) + ';' +
'function render(){' +
'  var stack=document.getElementById("stack");' +
'  stack.innerHTML="";' +
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
'    stack.appendChild(li);' +
'  });' +
'  var sw=document.getElementById("sw");sw.innerHTML="";' +
'  colors.forEach(function(c){' +
'    var d=document.createElement("div");' +
'    d.className="sw"+(c.argb===selColor?" sel":"");' +
'    d.style.background=c.css;d.title=c.name;' +
'    d.onclick=function(){selColor=c.argb;render();};' +
'    sw.appendChild(d);' +
'  });' +
'  [0,1].forEach(function(v){' +
'    var el=document.getElementById("st"+v);' +
'    el.className="style"+(v===selStyle?" sel":"");' +
'    el.onclick=function(){selStyle=v;render();};' +
'  });' +
'  ["C","F"].forEach(function(v){' +
'    var el=document.getElementById("un"+v);' +
'    el.className="style"+(v===selUnits?" sel":"");' +
'    el.onclick=function(){selUnits=v;render();};' +
'  });' +
'}' +
'render();' +
'document.getElementById("save").onclick=function(){' +
'  var payload=encodeURIComponent(JSON.stringify({order:order,color:selColor,style:selStyle,units:selUnits}));' +
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
    var msg = {};
    if (cfg && Array.isArray(cfg.order) && cfg.order.length === WIDGETS.length) {
      localStorage.setItem('slotOrderV2', JSON.stringify(cfg.order));
      msg.SlotOrder = cfg.order;
    }
    if (cfg && typeof cfg.color === 'number') {
      var argb = cfg.color & 0xFF;
      localStorage.setItem('progressColor', String(argb));
      msg.ProgressColor = argb;
    }
    if (cfg && (cfg.style === 0 || cfg.style === 1)) {
      localStorage.setItem('barStyle', String(cfg.style));
      msg.BarStyle = cfg.style;
    }
    var unitsChanged = false;
    if (cfg && (cfg.units === 'C' || cfg.units === 'F')) {
      var prevUnits = getSavedUnits();
      localStorage.setItem('weatherUnits', cfg.units);
      msg.WeatherUnits = cfg.units;
      unitsChanged = (prevUnits !== cfg.units);
    }
    if (Object.keys(msg).length) {
      Pebble.sendAppMessage(msg,
        function() {
          console.log('Settings sent: ' + JSON.stringify(msg));
          if (unitsChanged) fetchWeather();
        },
        function(err) { console.log('Settings send failed: ' + JSON.stringify(err)); });
    } else if (unitsChanged) {
      fetchWeather();
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
