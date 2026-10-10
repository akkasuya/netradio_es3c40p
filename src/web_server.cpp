#include "web_server.h"
#include <SD.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

WebServer server(80);

// ===== WEB PAGE =====
void handleRoot() {
    String html = "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<title>CYD-GOLD Manager</title>";

 // --- CSS STYLE GOLD ---
    html += "<style>";
    html += "body{background:#121212;color:#e0e0e0;font-family:'Segoe UI',sans-serif;margin:0;padding-bottom:80px;}";
    html += ".header{position:fixed;top:0;width:100%;background:#1a1a1a;padding:10px;text-align:center;box-shadow:0 2px 10px rgba(0,0,0,0.8);z-index:1000;}";
    html += ".btn{background:#d4af37;color:black;padding:10px 15px;border-radius:6px;text-decoration:none;font-weight:bold;display:inline-block;border:none;cursor:pointer;transition:0.3s;}";
    html += ".btn:hover{background:#ffcc33;}";
    html += ".btn-reboot{background:#3498db;color:white;font-size:0.8em;padding:8px 15px;}";
    html += ".alert{background:#443300;color:#ffcc33;padding:10px;border-left:5px solid #d4af37;margin:100px 20px 20px 20px;font-size:0.9em;border-radius:4px;}";
    html += ".nav-bar{position:fixed;bottom:0;width:100%;background:#1a1a1a;display:flex;justify-content:space-around;padding:15px 0;border-top:1px solid #333;z-index:1000;}";
    html += ".nav-item{color:#888;text-decoration:none;font-size:0.9em;display:flex;flex-direction:column;align-items:center;background:none;border:none;cursor:pointer;}";
    html += ".nav-item.active{color:#d4af37;}";
    html += ".section{display:none;padding:20px;animation: fadeIn 0.3s;}";
    html += "@keyframes fadeIn {from {opacity:0;} to {opacity:1;}}";
    html += ".card{background:#1e1e1e;padding:15px;border-radius:12px;margin-bottom:20px;border:1px solid #333;}";
    html += "input[type=text],input[type=file]{background:#2a2a2a;border:1px solid #444;color:white;padding:10px;width:100%;margin:10px 0;border-radius:5px;}";
    html += "table{width:100%;border-collapse:collapse;} td{padding:12px;border-bottom:1px solid #333;} .btn-del{color:#e74c3c;text-decoration:none;font-size:0.9em;}";

    // Gold upload button
    html += ".btn-upload {background:linear-gradient(145deg,#d4af37,#b8860b);color:black!important;border-radius:8px;box-shadow:0 4px #996600;transition:0.2s;font-weight:bold;padding:12px;}";
    html += ".btn-upload:active {box-shadow:0 2px #996600;transform:translateY(2px);}";

    html += "</style>";

    // --- JAVASCRIPT FOR TABS ---
    html += "<script>";
    html += "function showTab(id, el){";
    html += "  document.querySelectorAll('.section').forEach(s => s.style.display='none');";
    html += "  document.getElementById(id).style.display='block';";
    html += "  document.querySelectorAll('.nav-item').forEach(n => n.classList.remove('active'));";
    html += "  el.classList.add('active');";
    html += "}";
    html += "window.onload = function() {";
    html += "  if(window.location.hash == '#tab-mp3') {";
    html += "    showTab('tab-mp3', document.querySelectorAll('.nav-item')[1]);";
    html += "  }";
    html += "};";
    html += "</script>";

    html += "</head><body>";

    // --- HEADER ---
    html += "<div class='header'>";
    html += "<span style='color:#d4af37;font-weight:bold;font-size:1.2em;'>*** CYD-GOLD Manager ***</span> ";
    html += "<a href='/reboot' class='btn btn-reboot'>🔄 REBOOT</a>";
    html += "</div>";

    html += "<div class='alert'><b>Note:</b> Reboot the device after editing radios to refresh the alarm screen.</div>";

    // --- SECTION 1: RADIOS ---
    html += "<div id='tab-radio' class='section' style='display:block;'>";

    html += "<div class='card'><h3>📻 Add a Radio</h3>";
    html += "<form method='POST' action='/add_radio'>";
    html += "<input type='text' name='name' placeholder='Name (e.g. Fun Radio)' required>";
    html += "<input type='text' name='url' placeholder='Stream URL' required>";
    html += "<input type='text' name='logo' placeholder='Logo name (e.g. fun.bin)' required>";
    html += "<input type='submit' value='Save Radio' class='btn btn-upload' style='width:100%;font-weight:bold;'>";
    html += "</form></div>";

    html += "<div class='card'><h3>🎨 Upload a Logo</h3>";

    html += "<div style='margin-bottom:20px; padding:12px; background:#1a1a1a; border-left:4px solid #3498db; font-size:0.85em; color:#ccc; line-height:1.4;'>";
    html += "<strong style='color:#3498db;'>ℹ️ Creation guide:</strong><br>";
    html += "1. Prepare a <b>100px/100px</b> PNG image<br>";
    html += "2. Go to <a href='https://lvgl.io/tools/imageconverter' target='_blank' style='color:#3498db;'>LVGL Converter</a> (v8)<br>";
    html += "3. Color format: <b>CF_RGB565A8</b><br>";
    html += "4. Output format: <b>RGB565 binary</b><br>";
    html += "5. Convert, then upload the .bin here";
    html += "</div>";

    html += "<form method='POST' action='/upload_logo' enctype='multipart/form-data'>";
    html += "<input type='file' name='logo_file' accept='.bin'>";
    html += "<input type='submit' value='Save Logo' class='btn btn-upload' style='width:100%;font-weight:bold;'>";
    html += "</form></div>";

    html += "<div class='card'><h3>📂 Logos on SD</h3><p style='font-size:0.8em; color:#aaa;'>";
    File logoDir = SD.open("/logos");
    if (logoDir) {
        File f = logoDir.openNextFile();
        while (f) {
            html += String(f.name()) + " | ";
            f = logoDir.openNextFile();
        }
        logoDir.close();
    }
    html += "</p></div>";

    html += "<div class='card'><h3>📋 Saved Radios</h3><table>";
    File radioFile = LittleFS.open("/radios.json", FILE_READ);
    if (radioFile) {
        JsonDocument doc;
        deserializeJson(doc, radioFile);
        radioFile.close();
        JsonArray array = doc.as<JsonArray>();
        for (int i = 0; i < array.size(); i++) {
            String rName = array[i]["name"] | "Unknown";
            html += "<tr><td>" + rName + "</td><td style='text-align:right;'>";
            html += "<a href='/delete_radio?id=" + String(i) + "' class='btn-del'>Delete</a></td></tr>";
        }
    }
    html += "</table></div></div>";

    // --- SECTION 2: MP3 ---
    html += "<div id='tab-mp3' class='section'>";
    html += "<div class='card'><h3>🎵 Upload an MP3</h3>";
    html += "<form method='POST' action='/update' enctype='multipart/form-data'>";
    html += "<input type='file' name='update' accept='.mp3'>";
    html += "<input type='submit' value='Save to SD /mp3' class='btn btn-upload' style='width:100%;font-weight:bold;'>";
    html += "</form></div>";

    html += "<div class='card'><h3>📂 Files on SD</h3><ul style='list-style:none; padding:0;'>";
    File rootDir = SD.open("/mp3");
        if (rootDir) {
        File f = rootDir.openNextFile();
        while (f) {
            if (!f.isDirectory()) {
                String fileName = String(f.name());
                html += "<li style='margin-bottom:15px; border-bottom:1px solid #333; padding-bottom:5px; display:flex; justify-content:space-between; align-items:center;'>";
                html += "<span>" + fileName + "</span>";
                html += "<a href='/delete?file=" + fileName + "' class='btn-del' onclick='return confirm(\"Delete " + fileName + " ?\")'>Delete</a>";
                html += "</li>";
            }
        f = rootDir.openNextFile();
        }
        rootDir.close();
    }
    html += "</ul></div></div>";

    // --- FIXED BOTTOM NAVIGATION ---
    html += "<div class='nav-bar'>";
    html += "<button class='nav-item active' onclick='showTab(\"tab-radio\", this)'><span>📻</span><span>RADIOS</span></button>";
    html += "<button class='nav-item' onclick='showTab(\"tab-mp3\", this)'><span>🎵</span><span>MP3</span></button>";
    html += "</div>";

    html += "</body></html>";
    server.send(200, "text/html", html);
}


// ===== WEB SERVER OPERATION =====
void setup_web_server() {
    server.on("/", handleRoot);

    // --- delete mp3 function ---
    server.on("/delete", []() {
        String fileName = server.arg("file");
        String path = "/mp3/" + fileName;
        if (SD.remove(path)) {
            Serial.println("File deleted: " + path);
        }
        server.sendHeader("Location", "/#tab-mp3");
        server.send(303);
    });

    // --- upload mp3 function ---
    server.on("/update", HTTP_POST, []() {
    // Send a small HTML page instead of plain text
    String response = "<html><body style='background:#121212;color:#d4af37;text-align:center;font-family:sans-serif;padding-top:50px;'>";
    response += "<h2>File received!</h2>";
    response += "<p>Updating SD card...</p>";
    response += "<p>CYD-GOLD rebooting in 5 seconds...</p>";
    response += "<script>setTimeout(function(){ window.location.href = '/'; }, 5000);</script>";
    response += "</body></html>";

    server.send(200, "text/html", response);

    delay(1000);
    ESP.restart();
    }, []() {
        HTTPUpload& upload = server.upload();
        if (upload.status == UPLOAD_FILE_START) {
            String filename = upload.filename;
            if (!filename.startsWith("/")) filename = "/" + filename;
            Serial.printf("Upload: %s\n", filename.c_str());
            File file = SD.open("/mp3" + filename, FILE_WRITE);
            if (file) file.close();
        } else if (upload.status == UPLOAD_FILE_WRITE) {
            String filename = upload.filename;
            if (!filename.startsWith("/")) filename = "/" + filename;
            File file = SD.open("/mp3" + filename, FILE_APPEND);
            if (file) {
                file.write(upload.buf, upload.currentSize);
                file.close();
            }
        }
    });

    // --- add radio function ---
    server.on("/add_radio", HTTP_POST, []() {
    String name = server.arg("name");
    String url = server.arg("url");
    String logoRaw = server.arg("logo");

    String finalLogoPath;
    if (logoRaw.startsWith("S:/logos/")) {
        finalLogoPath = logoRaw; // Already prefixed, leave as is
    } else {
        finalLogoPath = "S:/logos/" + logoRaw; // Add prefix for them
    }

      if (name != "" && url != "") {
        // 1. Read current file
        File file = LittleFS.open("/radios.json", FILE_READ);
        JsonDocument doc;

        if (file) {
            deserializeJson(doc, file);
            file.close();
        }

        // 2. Add the new radio
        JsonObject obj = doc.add<JsonObject>();
        obj["name"] = name;
        obj["url"] = url;
        obj["logo"] = finalLogoPath;

        // 3. Save to flash
        file = LittleFS.open("/radios.json", FILE_WRITE);
        if (file) {
            serializeJson(doc, file);
            file.close();
            Serial.println("Radio added to JSON!");

            // 4. Redirect home to reload the list
            server.sendHeader("Location", "/");
            server.send(303);

            // Optional: force station reload here without rebooting
            // loadStationsFromSD();
        } else {
            server.send(500, "text/plain", "SD write error");
        }
      }
    });

    // --- delete radio function ---
    server.on("/delete_radio", HTTP_GET, []() {
      if (server.hasArg("id")) {
        int id = server.arg("id").toInt();

        File file = LittleFS.open("/radios.json", FILE_READ);
        JsonDocument doc;
        if (file) {
            deserializeJson(doc, file);
            file.close();
        }

        JsonArray array = doc.as<JsonArray>();
        if (id >= 0 && id < array.size()) {
            array.remove(id); // Delete the radio at the given index

            file = LittleFS.open("/radios.json", FILE_WRITE);
            if (file) {
                serializeJson(doc, file);
                file.close();
                Serial.printf("Radio index %d deleted\n", id);
            }
        }
      }
      server.sendHeader("Location", "/");
      server.send(303);
    });

    // --- upload logo function ---
    server.on("/upload_logo", HTTP_POST, []() {
        server.sendHeader("Location", "/");
        server.send(303);
    }, []() {
        HTTPUpload& upload = server.upload();
        if (upload.status == UPLOAD_FILE_START) {
            String filename = upload.filename;
            if (!filename.startsWith("/")) filename = "/" + filename;
            Serial.printf("Upload Logo: %s\n", filename.c_str());
            // Force the /logos folder
            File file = SD.open("/logos" + filename, FILE_WRITE);
            if (file) file.close();
        } else if (upload.status == UPLOAD_FILE_WRITE) {
            String filename = upload.filename;
            if (!filename.startsWith("/")) filename = "/" + filename;
            File file = SD.open("/logos" + filename, FILE_APPEND);
            if (file) {
                file.write(upload.buf, upload.currentSize);
                file.close();
            }
        }
    });

    // --- reboot function ---
    server.on("/reboot", []() {
    server.send(200, "text/html", "<html><body style='background:#1a1a1a;color:white;text-align:center;font-family:sans-serif;'><h1>Rebooting...</h1><p>AudioCYD-GOLD will be back in a few seconds.</p><script>setTimeout(function(){location.href='/';}, 5000);</script></body></html>");
    delay(1000);
    ESP.restart();
    });

    // --- start server ---
    server.begin();
    Serial.println("Web server started!");
}
