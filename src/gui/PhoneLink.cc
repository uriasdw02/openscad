#include "gui/PhoneLink.h"

#include <set>
#include <sstream>
#include <string>

#include <QAction>
#include <QBuffer>
#include <QCoreApplication>
#include <QFileInfo>
#include <QHostAddress>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QMessageBox>
#include <QNetworkInterface>
#include <QRandomGenerator>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUrl>
#include <QUrlQuery>

#include "geometry/Geometry.h"
#include "gui/Editor.h"
#include "gui/MainWindow.h"
#include "gui/QGLView.h"
#include "gui/input/InputDriverEvent.h"
#include "io/export.h"

namespace {

constexpr qsizetype kMaxRequestBytes = 4 * 1024 * 1024;  // 4 MB: plenty for .scad source

// Only these menu actions can be triggered from the phone.
const std::set<std::string>& allowedActions()
{
  static const std::set<std::string> actions = {
    "designActionPreview", "designActionReloadAndPreview", "designActionRender",
    "viewActionViewAll",   "viewActionResetView",          "viewActionCenter",
    "viewActionTop",       "viewActionBottom",             "viewActionLeft",
    "viewActionRight",     "viewActionFront",              "viewActionBack",
    "viewActionDiagonal",  "viewActionZoomIn",             "viewActionZoomOut",
    "viewActionShowEdges", "viewActionShowAxes",           "viewActionPerspective",
    "viewActionOrthogonal",
  };
  return actions;
}

QByteArray statusText(int status)
{
  switch (status) {
  case 200: return "OK";
  case 400: return "Bad Request";
  case 403: return "Forbidden";
  case 404: return "Not Found";
  case 409: return "Conflict";
  case 413: return "Payload Too Large";
  default:  return "Error";
  }
}

}  // namespace

PhoneLink::PhoneLink(QObject *parent) : QObject(parent) {}

PhoneLink *PhoneLink::instance()
{
  static PhoneLink *link = new PhoneLink(qApp);
  return link;
}

void PhoneLink::install(MainWindow *window)
{
  auto *link = instance();
  window->menu_Design->addSeparator();
  QAction *action = window->menu_Design->addAction(tr("Phone Link..."));
  action->setObjectName("designActionPhoneLink");
  action->setCheckable(true);
  action->setChecked(link->isRunning());
  action->setToolTip(tr("Control this window from your phone's browser over Wi-Fi"));

  QObject::connect(link, &PhoneLink::runningChanged, action, [action](bool running) {
    const QSignalBlocker block(action);
    action->setChecked(running);
  });

  QObject::connect(action, &QAction::triggered, window, [window, link](bool checked) {
    if (!checked) {
      link->stop();
      return;
    }
    QString error;
    if (!link->start(window, &error)) {
      QMessageBox::warning(window, tr("Phone Link"), tr("Could not start Phone Link:\n%1").arg(error));
      return;
    }
    QMessageBox::information(
      window, tr("Phone Link"),
      tr("<p>Phone Link is on.</p>"
         "<p>On your phone (same Wi-Fi as this PC), open:</p>"
         "<p style='font-size:16pt'><b>%1</b></p>"
         "<p>PIN: <b>%2</b> (already included in the link)</p>"
         "<p>If Windows asks about the firewall, allow access on <i>private</i> networks.</p>")
        .arg(link->url().toHtmlEscaped(), link->pin()));
  });
}

bool PhoneLink::isRunning() const { return server_ && server_->isListening(); }

bool PhoneLink::start(MainWindow *window, QString *error)
{
  // If it's already running, just point it at the window that asked.
  if (isRunning()) {
    window_ = window;
    return true;
  }
  if (!server_) {
    server_ = new QTcpServer(this);
    connect(server_, &QTcpServer::newConnection, this, &PhoneLink::onNewConnection);
  }
  if (!server_->listen(QHostAddress::AnyIPv4, port_)) {
    if (error) *error = server_->errorString();
    return false;
  }
  window_ = window;
  connect(window, &QObject::destroyed, this, [this](QObject *obj) {
    if (window_ && obj == static_cast<QObject *>(window_)) stop();
  });
  pin_ = QString::number(QRandomGenerator::global()->bounded(1000, 10000));
  emit runningChanged(true);
  return true;
}

void PhoneLink::stop()
{
  if (server_) server_->close();
  // abort() fires disconnected(), which edits buffers_, so work on a copy.
  const auto sockets = buffers_.keys();
  buffers_.clear();
  for (QTcpSocket *socket : sockets) socket->abort();
  window_ = nullptr;
  emit runningChanged(false);
}

QString PhoneLink::localAddress()
{
  // Prefer a private LAN address (192.168.x.x / 10.x.x.x / 172.16-31.x.x).
  QString fallback;
  for (const QHostAddress& addr : QNetworkInterface::allAddresses()) {
    if (addr.protocol() != QAbstractSocket::IPv4Protocol || addr.isLoopback()) continue;
    const quint32 ip = addr.toIPv4Address();
    const bool isPrivate = (ip >> 24) == 10 || (ip >> 16) == 0xC0A8 || (ip >> 20) == 0xAC1;
    if (isPrivate) return addr.toString();
    if (fallback.isEmpty()) fallback = addr.toString();
  }
  return fallback.isEmpty() ? QStringLiteral("127.0.0.1") : fallback;
}

QString PhoneLink::url() const
{
  return QStringLiteral("http://%1:%2/?k=%3").arg(localAddress()).arg(port_).arg(pin_);
}

void PhoneLink::onNewConnection()
{
  while (QTcpSocket *socket = server_->nextPendingConnection()) {
    buffers_.insert(socket, {});
    connect(socket, &QTcpSocket::readyRead, this, [this, socket] { onReadyRead(socket); });
    connect(socket, &QTcpSocket::disconnected, this, [this, socket] {
      buffers_.remove(socket);
      socket->deleteLater();
    });
  }
}

void PhoneLink::onReadyRead(QTcpSocket *socket)
{
  auto it = buffers_.find(socket);
  if (it == buffers_.end()) return;
  QByteArray& buf = it.value();
  buf += socket->readAll();

  if (buf.size() > kMaxRequestBytes) {
    reply(socket, 413, "text/plain", "Request too large");
    return;
  }
  const qsizetype headerEnd = buf.indexOf("\r\n\r\n");
  if (headerEnd < 0) return;  // wait for the rest of the headers

  const QList<QByteArray> lines = buf.left(headerEnd).split('\n');
  const QList<QByteArray> requestLine = lines.value(0).trimmed().split(' ');
  if (requestLine.size() < 2) {
    reply(socket, 400, "text/plain", "Bad request");
    return;
  }
  qsizetype contentLength = 0;
  for (qsizetype i = 1; i < lines.size(); ++i) {
    const QByteArray line = lines[i].trimmed();
    if (line.toLower().startsWith("content-length:")) {
      contentLength = line.mid(15).trimmed().toLongLong();
    }
  }
  if (contentLength < 0 || contentLength > kMaxRequestBytes) {
    reply(socket, 413, "text/plain", "Request too large");
    return;
  }
  const qsizetype bodyStart = headerEnd + 4;
  if (buf.size() - bodyStart < contentLength) return;  // wait for the body

  const QByteArray body = buf.mid(bodyStart, contentLength);
  const QByteArray method = requestLine[0];
  const QByteArray target = requestLine[1];
  buf.clear();
  handleRequest(socket, method, target, body);
}

void PhoneLink::handleRequest(QTcpSocket *socket, const QByteArray& method, const QByteArray& target,
                              const QByteArray& body)
{
  const QUrl url(QStringLiteral("http://phone") + QString::fromUtf8(target));
  const QUrlQuery query(url);
  const QString path = url.path();

  if (path == "/" || path == "/index.html") {
    reply(socket, 200, "text/html; charset=utf-8", pageHtml());
    return;
  }
  if (!path.startsWith("/api/")) {
    reply(socket, 404, "text/plain", "Not found");
    return;
  }
  if (query.queryItemValue("k") != pin_) {
    replyJson(socket, 403, R"({"error":"Wrong or missing PIN. Re-open the link shown on the PC."})");
    return;
  }
  if (!window_) {
    replyJson(socket, 409, R"({"error":"No OpenSCAD window is linked."})");
    return;
  }

  const bool isPost = method == "POST";

  if (path == "/api/status") {
    replyJson(socket, 200, statusJson());
  } else if (path == "/api/view.png") {
    QImage frame = window_->qglview->grabFrame();
    if (frame.width() > 1280 || frame.height() > 1280) {
      frame = frame.scaled(1280, 1280, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    QByteArray png;
    QBuffer out(&png);
    out.open(QIODevice::WriteOnly);
    frame.save(&out, "PNG");
    reply(socket, 200, "image/png", png, "Cache-Control: no-store\r\n");
  } else if (path == "/api/action" && isPost) {
    const std::string name = query.queryItemValue("name").toStdString();
    if (!allowedActions().count(name)) {
      replyJson(socket, 400, R"({"error":"Unknown action"})");
      return;
    }
    InputEventAction event(name, false);
    window_->onActionEvent(&event);
    replyJson(socket, 200, R"({"ok":true})");
  } else if (path == "/api/rotate" && isPost) {
    const double dx = query.queryItemValue("dx").toDouble();
    const double dy = query.queryItemValue("dy").toDouble();
    window_->qglview->rotate(dy, 0.0, dx, true);
    replyJson(socket, 200, R"({"ok":true})");
  } else if (path == "/api/zoom" && isPost) {
    window_->qglview->zoom(query.queryItemValue("d").toDouble(), true);
    replyJson(socket, 200, R"({"ok":true})");
  } else if (path == "/api/code" && !isPost) {
    const QString code = window_->activeEditor ? window_->activeEditor->toPlainText() : QString();
    reply(socket, 200, "text/plain; charset=utf-8", code.toUtf8());
  } else if (path == "/api/code" && isPost) {
    if (!window_->activeEditor) {
      replyJson(socket, 409, R"({"error":"No editor open"})");
      return;
    }
    window_->activeEditor->setPlainText(QString::fromUtf8(body));
    if (query.queryItemValue("preview") == "1") {
      InputEventAction event("designActionPreview", false);
      window_->onActionEvent(&event);
    }
    replyJson(socket, 200, R"({"ok":true})");
  } else if (path == "/api/model.stl") {
    const auto geom = window_->rootGeom;
    if (!geom || geom->getDimension() != 3 || geom->isEmpty()) {
      replyJson(socket, 409, R"({"error":"No 3D model yet. Press Render (F6) first."})");
      return;
    }
    std::ostringstream stl;
    export_stl(geom, stl, true);
    const std::string data = stl.str();
    QString base = "model";
    if (window_->activeEditor && !window_->activeEditor->filepath.isEmpty()) {
      base = QFileInfo(window_->activeEditor->filepath).completeBaseName();
    }
    reply(socket, 200, "model/stl", QByteArray(data.data(), static_cast<qsizetype>(data.size())),
          QStringLiteral("Content-Disposition: attachment; filename=\"%1.stl\"\r\n").arg(base).toUtf8());
  } else {
    replyJson(socket, 404, R"({"error":"Unknown API call"})");
  }
}

QByteArray PhoneLink::statusJson() const
{
  QJsonObject obj;
  QString file = tr("Untitled");
  if (window_ && window_->activeEditor && !window_->activeEditor->filepath.isEmpty()) {
    file = QFileInfo(window_->activeEditor->filepath).fileName();
  }
  obj["file"] = file;
  const bool hasModel = window_ && window_->rootGeom && window_->rootGeom->getDimension() == 3 &&
                        !window_->rootGeom->isEmpty();
  obj["stlReady"] = hasModel;
  obj["errors"] = window_ ? window_->compileErrors : 0;
  obj["warnings"] = window_ ? window_->compileWarnings : 0;
  return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}

void PhoneLink::reply(QTcpSocket *socket, int status, const QByteArray& contentType,
                      const QByteArray& body, const QByteArray& extraHeaders)
{
  QByteArray head = "HTTP/1.1 " + QByteArray::number(status) + " " + statusText(status) + "\r\n";
  head += "Content-Type: " + contentType + "\r\n";
  head += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
  head += extraHeaders;
  head += "Connection: close\r\n\r\n";
  socket->write(head);
  socket->write(body);
  socket->disconnectFromHost();
}

void PhoneLink::replyJson(QTcpSocket *socket, int status, const QByteArray& json)
{
  reply(socket, status, "application/json", json, "Cache-Control: no-store\r\n");
}

QByteArray PhoneLink::pageHtml()
{
  static const char *html = R"HTML(<!doctype html>
<html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
<meta name="theme-color" content="#16181d">
<title>OpenSCAD Phone Link</title>
<style>
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
html,body{margin:0;background:#16181d;color:#e8e8e8;font-family:system-ui,sans-serif;overscroll-behavior:none}
header{display:flex;justify-content:space-between;align-items:center;padding:10px 14px;background:#20232a}
header b{font-size:16px}#st{font-size:12px;color:#9aa}
.tabs{display:flex;background:#20232a;border-top:1px solid #2c3038}
.tabs button{flex:1;padding:12px;background:none;border:0;color:#9aa;font-size:15px}
.tabs button.on{color:#f9d72c;border-bottom:2px solid #f9d72c}
section{display:none;padding:10px}section.on{display:block}
#view{width:100%;height:auto;min-height:200px;object-fit:contain;background:#000;border-radius:8px;
 touch-action:none;display:block;user-select:none;-webkit-user-select:none}
.row{display:grid;grid-template-columns:repeat(4,1fr);gap:8px;margin-top:8px}
.row.two{grid-template-columns:1fr 1fr}
button.b{padding:13px 4px;border:0;border-radius:8px;background:#2c3038;color:#eee;font-size:14px}
button.p{background:#f9d72c;color:#111;font-weight:600}
button.g{background:#2f7d4f;color:#fff;font-weight:600}
textarea{width:100%;height:60vh;background:#0f1115;color:#e8e8e8;border:1px solid #2c3038;border-radius:8px;
 padding:10px;font:13px/1.4 ui-monospace,Consolas,monospace}
#msg{position:fixed;left:50%;bottom:18px;transform:translateX(-50%);background:#333;padding:10px 16px;
 border-radius:20px;font-size:14px;opacity:0;transition:opacity .3s;pointer-events:none;z-index:30}
label{font-size:13px;color:#9aa;display:flex;gap:6px;align-items:center;margin-top:10px}
.hint{font-size:12px;color:#778;margin-top:6px}
/* Full-screen mode: only the 3D view, plus a small toolbar that a tap shows/hides */
#bar{display:none}
body.full header,body.full .tabs,body.full .ctl{display:none}
body.full section#v{padding:0}
body.full #view{position:fixed;inset:0;width:100vw;height:100vh;height:100dvh;border-radius:0;min-height:0}
body.full #bar{display:flex;position:fixed;left:8px;right:8px;bottom:max(10px,env(safe-area-inset-bottom));
 gap:6px;justify-content:center;flex-wrap:wrap;z-index:20;transition:opacity .25s}
body.full.nobar #bar{opacity:0;pointer-events:none}
body.full #msg{bottom:auto;top:max(18px,env(safe-area-inset-top))}
#bar button{padding:10px 12px;border:0;border-radius:20px;background:rgba(32,35,42,.82);color:#eee;font-size:14px;
 backdrop-filter:blur(4px)}
#bar button.p{background:rgba(249,215,44,.92);color:#111;font-weight:600}
</style></head><body>
<header><b id="file">OpenSCAD</b><span id="st">connecting...</span></header>
<div class="tabs"><button class="on" data-t="v">View</button><button data-t="c">Code</button></div>
<section id="v" class="on">
 <img id="view" alt="viewport" draggable="false">
 <div class="ctl">
 <div class="row two"><button class="b p" data-a="designActionPreview">Preview (F5)</button>
  <button class="b p" data-a="designActionRender">Render (F6)</button></div>
 <div class="row"><button class="b" data-a="viewActionTop">Top</button><button class="b" data-a="viewActionFront">Front</button>
  <button class="b" data-a="viewActionRight">Right</button><button class="b" data-a="viewActionDiagonal">Iso</button></div>
 <div class="row"><button class="b" data-z="120">Zoom +</button><button class="b" data-z="-120">Zoom -</button>
  <button class="b" data-a="viewActionViewAll">Fit</button><button class="b" data-a="viewActionResetView">Reset</button></div>
 <div class="row two"><button class="b g" id="stl">Download STL</button><button class="b" id="fs">&#x26F6; Full screen</button></div>
 <label><input type="checkbox" id="live" checked> Live view (updates every 2s)</label>
 <div class="hint">Drag to rotate &middot; pinch to zoom &middot; double-tap to fit</div>
 </div>
</section>
<section id="c">
 <textarea id="code" spellcheck="false" autocapitalize="off" autocorrect="off"></textarea>
 <div class="row two"><button class="b" id="pull">Load from PC</button><button class="b p" id="push">Send + Preview</button></div>
</section>
<div id="bar">
 <button id="fsx">&#x2715; Exit</button>
 <button class="p" data-a="designActionPreview">F5</button>
 <button class="p" data-a="designActionRender">F6</button>
 <button data-a="viewActionDiagonal">Iso</button>
 <button data-a="viewActionTop">Top</button>
 <button data-a="viewActionFront">Front</button>
 <button data-a="viewActionViewAll">Fit</button>
</div>
<div id="msg"></div>
<script>
const k=new URLSearchParams(location.search).get('k')||'';
const q=(p)=>p+(p.includes('?')?'&':'?')+'k='+encodeURIComponent(k);
const $=(id)=>document.getElementById(id);
function toast(t){const m=$('msg');m.textContent=t;m.style.opacity=1;clearTimeout(m._t);m._t=setTimeout(()=>m.style.opacity=0,2200);}
async function call(p,opt){const r=await fetch(q(p),opt);if(!r.ok){let e='Error '+r.status;try{e=(await r.json()).error||e}catch(_){};throw new Error(e);}return r;}
let busy=false;
function refresh(){if(busy)return;busy=true;const img=new Image();
 img.onload=()=>{$('view').src=img.src;busy=false};img.onerror=()=>{busy=false};
 img.src=q('/api/view.png?t='+Date.now());}
function soon(){refresh();setTimeout(refresh,700);setTimeout(refresh,2000);}
async function status(){try{const s=await (await call('/api/status')).json();$('file').textContent=s.file;
 $('st').textContent=(s.errors?s.errors+' errors · ':'')+(s.stlReady?'STL ready':'connected');}
 catch(e){$('st').textContent=e.message;}}
async function act(name){try{await call('/api/action?name='+name,{method:'POST'});soon();status();}catch(e){toast(e.message)}}
document.querySelectorAll('[data-a]').forEach(b=>b.onclick=()=>act(b.dataset.a));
document.querySelectorAll('[data-z]').forEach(b=>b.onclick=async()=>{try{await call('/api/zoom?d='+b.dataset.z,{method:'POST'});refresh();}catch(e){toast(e.message)}});
$('stl').onclick=async()=>{try{const s=await (await call('/api/status')).json();
 if(!s.stlReady){toast('Press Render (F6) first');return;}location.href=q('/api/model.stl');}catch(e){toast(e.message)}};
document.querySelectorAll('.tabs button').forEach(b=>b.onclick=()=>{
 document.querySelectorAll('.tabs button,section').forEach(x=>x.classList.remove('on'));
 b.classList.add('on');$(b.dataset.t).classList.add('on');if(b.dataset.t==='c'&&!$('code').value)pull();});
async function pull(){try{$('code').value=await (await call('/api/code')).text();toast('Loaded from PC');}catch(e){toast(e.message)}}
$('pull').onclick=pull;
$('push').onclick=async()=>{try{await call('/api/code?preview=1',{method:'POST',body:$('code').value});toast('Sent to PC');soon();status();}catch(e){toast(e.message)}};

// ---- Full screen ----
function setFull(on){document.body.classList.toggle('full',on);document.body.classList.remove('nobar');
 const de=document.documentElement;
 try{if(on&&de.requestFullscreen&&!document.fullscreenElement)de.requestFullscreen().catch(()=>{});
  if(!on&&document.fullscreenElement)document.exitFullscreen().catch(()=>{});}catch(_){}
 if(on)toast('Tap the model to show/hide buttons');refresh();}
$('fs').onclick=()=>setFull(true);
$('fsx').onclick=()=>setFull(false);
document.addEventListener('fullscreenchange',()=>{if(!document.fullscreenElement)document.body.classList.remove('full');});

// ---- Touch: 1 finger rotates, 2 fingers pinch-zoom, tap toggles toolbar, double-tap fits ----
const v=$('view'),pts=new Map();let acc=[0,0],zacc=0,sending=false,pinch=0,down=null,lastTap=0,moved=0;
v.addEventListener('pointerdown',e=>{v.setPointerCapture(e.pointerId);pts.set(e.pointerId,[e.clientX,e.clientY]);
 if(pts.size===1){down={t:Date.now()};moved=0;}else{pinch=dist();}});
function dist(){const a=[...pts.values()];return a.length<2?0:Math.hypot(a[0][0]-a[1][0],a[0][1]-a[1][1]);}
v.addEventListener('pointermove',e=>{const p=pts.get(e.pointerId);if(!p)return;
 const dx=e.clientX-p[0],dy=e.clientY-p[1];pts.set(e.pointerId,[e.clientX,e.clientY]);moved+=Math.abs(dx)+Math.abs(dy);
 if(pts.size===1){acc[0]+=dx;acc[1]+=dy;}
 else if(pts.size===2){const d=dist();zacc+=(d-pinch)*3;pinch=d;}});
function up(e){pts.delete(e.pointerId);if(pts.size===1)pinch=0;
 if(pts.size===0&&down){const quick=Date.now()-down.t<300&&moved<10;down=null;
  if(quick){const now=Date.now();
   if(now-lastTap<350){lastTap=0;act('viewActionViewAll');}
   else{lastTap=now;setTimeout(()=>{if(lastTap===now&&document.body.classList.contains('full'))document.body.classList.toggle('nobar');},360);}}}}
v.addEventListener('pointerup',up);v.addEventListener('pointercancel',up);
setInterval(async()=>{if(sending||(!acc[0]&&!acc[1]&&!zacc))return;sending=true;
 const dx=acc[0]*0.7,dy=acc[1]*0.7,dz=zacc;acc=[0,0];zacc=0;
 try{if(dx||dy)await call('/api/rotate?dx='+dx.toFixed(1)+'&dy='+dy.toFixed(1),{method:'POST'});
  if(dz)await call('/api/zoom?d='+dz.toFixed(0),{method:'POST'});refresh();}catch(e){toast(e.message)}
 sending=false;},150);
setInterval(()=>{if($('live').checked&&!pts.size&&!document.hidden)refresh();},2000);
setInterval(()=>{if(!document.hidden)status();},4000);
refresh();status();
</script></body></html>)HTML";
  return QByteArray(html);
}
