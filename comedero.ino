#include <FS.h>
#include <LittleFS.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>
#include <TimeLib.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

WiFiClient espClient;
PubSubClient mqttClient(espClient);
ESP8266WebServer server(80);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", -3 * 3600, 60000); // GMT-3

const int ledPin = D1;
const char* configFile = "/config.json";
const char* horariosFile = "/horarios.json";

// Estructura de configuración
struct Config {
  String mqtt_host;
  int mqtt_port;
  String mqtt_client_id;
};
Config config;

// ----------------------
// Control del comedero (LED)
// ----------------------
void abrirComedero(int tiempoApertura = 3) {  // tiempoApertura en segundos
  Serial.println("Comedero abierto (LED encendido)");
  digitalWrite(ledPin, HIGH);
  delay(tiempoApertura * 1000);  // Mantener abierto el tiempo especificado
  cerrarComedero();
}

void cerrarComedero() {
  Serial.println("Comedero cerrado (LED apagado)");
  digitalWrite(ledPin, LOW);
}

// ----------------------
// Horarios
// ----------------------
struct Horario {
  int hora;
  int minuto;
};

std::vector<Horario> horarios;

bool cargarHorarios() {
  if (!LittleFS.exists(horariosFile)) return false;
  File file = LittleFS.open(horariosFile, "r");
  if (!file) return false;

  StaticJsonDocument<1024> doc;
  if (deserializeJson(doc, file)) {
    file.close();
    return false;
  }
  file.close();

  horarios.clear();
  for (JsonObject h : doc["horarios"].as<JsonArray>()) {
    horarios.push_back({h["hh"], h["mm"]});
  }
  return true;
}

bool guardarHorarios() {
  File file = LittleFS.open(horariosFile, "w");
  if (!file) return false;

  StaticJsonDocument<1024> doc;
  JsonArray arr = doc.createNestedArray("horarios");
  for (auto& h : horarios) {
    JsonObject o = arr.createNestedObject();
    o["hh"] = h.hora;
    o["mm"] = h.minuto;
  }

  bool ok = serializeJson(doc, file) > 0;
  file.close();
  return ok;
}

void verificarHorarios() {
  static int ultimaHora = -1, ultimoMinuto = -1;
  int hh = hour(), mm = minute();

  if (hh != ultimaHora || mm != ultimoMinuto) {
    for (auto& h : horarios) {
      if (h.hora == hh && h.minuto == mm) {
        abrirComedero();
        break;
      }
    }
    ultimaHora = hh;
    ultimoMinuto = mm;
  }
}

// ----------------------
// MQTT
// ----------------------
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (int i = 0; i < length; i++) message += (char)payload[i];
  Serial.printf("MQTT [%s]: %s\n", topic, message.c_str());

  if (String(topic) == "comedero/control") {
    if (message == "abrir") abrirComedero();
    else if (message == "cerrar") cerrarComedero();
  }

  else if (String(topic) == "comedero/horarios") {
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, payload, length);
    if (!error) {
      horarios.clear();
      JsonArray arr = doc["horarios"].as<JsonArray>();
      for (JsonObject h : arr) {
        int hora = h["hh"];
        int minuto = h["mm"];
        horarios.push_back({hora, minuto});
      }
      guardarHorarios();
      Serial.println("Horarios actualizados");
    }
  }
}

// ----------------------
// Configuración
// ----------------------
bool saveConfig() {
  File file = LittleFS.open(configFile, "w");
  if (!file) return false;

  StaticJsonDocument<512> doc;
  doc["mqtt_host"] = config.mqtt_host;
  doc["mqtt_port"] = config.mqtt_port;
  doc["mqtt_client_id"] = config.mqtt_client_id;

  bool success = serializeJson(doc, file) > 0;
  file.close();
  return success;
}

bool loadConfig() {
  if (!LittleFS.exists(configFile)) return false;
  File file = LittleFS.open(configFile, "r");
  if (!file) return false;

  StaticJsonDocument<512> doc;
  if (deserializeJson(doc, file)) {
    file.close();
    return false;
  }
  file.close();

  config.mqtt_host = doc["mqtt_host"].as<String>();
  config.mqtt_port = doc["mqtt_port"];
  config.mqtt_client_id = doc["mqtt_client_id"].as<String>();
  return true;
}

// ----------------------
// Setup
// ----------------------
void setup() {
  Serial.begin(115200);
  LittleFS.begin();
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  WiFiManager wm;
  wm.setTimeout(180);
  bool wifiOK = wm.autoConnect("ComederoSetup");

  if (!wifiOK) {
    Serial.println("No se pudo conectar al WiFi.");
    ESP.restart();
  }

  WiFiManagerParameter mqtt_host("host", "MQTT Host", "", 40);
  WiFiManagerParameter mqtt_port("port", "MQTT Port", "1883", 6);
  WiFiManagerParameter mqtt_client_id("clientid", "MQTT Client ID", "comedero001", 20);

  wm.addParameter(&mqtt_host);
  wm.addParameter(&mqtt_port);
  wm.addParameter(&mqtt_client_id);

  if (!loadConfig()) {
    Serial.println("Sin configuración previa. Usando portal.");
    wm.startConfigPortal("ComederoSetup");

    config.mqtt_host = mqtt_host.getValue();
    config.mqtt_port = atoi(mqtt_port.getValue());
    config.mqtt_client_id = mqtt_client_id.getValue();
    saveConfig();
  }

  mqttClient.setServer(config.mqtt_host.c_str(), config.mqtt_port);
  mqttClient.setCallback(mqttCallback);

  // Aquí, en lugar de bloquear con la conexión MQTT, solo intentamos conectar una vez y seguimos con el servidor web
  if (mqttClient.connect(config.mqtt_client_id.c_str())) {
    Serial.println("Conectado a MQTT");
    mqttClient.subscribe("comedero/control");
    mqttClient.subscribe("comedero/horarios");
  } else {
    Serial.println("No se pudo conectar a MQTT, el servidor web continuará funcionando.");
  }

  cargarHorarios();
  timeClient.begin();

  server.on("/", []() {
    server.send(200, "text/plain", "Comedero disponible. Usa /abrir, /abrir?tiempo=5, /cerrar, /agregarHorario?hh=8&mm=0");
  });

  server.on("/abrir", []() {
    int tiempo = 3;  // Default 5 segundos
    if (server.hasArg("tiempo")) {
      tiempo = server.arg("tiempo").toInt();
    }
    abrirComedero(tiempo);
    server.send(200, "text/plain", "Comedero abierto por " + String(tiempo) + " segundos");
  });

  server.on("/cerrar", []() {
    cerrarComedero();
    server.send(200, "text/plain", "Comedero cerrado");
  });

  server.on("/agregarHorario", []() {
    if (!server.hasArg("hh") || !server.hasArg("mm")) {
      server.send(400, "text/plain", "Faltan parámetros hh y mm");
      return;
    }

    int hh = server.arg("hh").toInt();
    int mm = server.arg("mm").toInt();
    if (hh < 0 || hh > 23 || mm < 0 || mm > 59) {
      server.send(400, "text/plain", "Hora inválida");
      return;
    }
    // Verificar duplicado
    for (auto& h_exist : horarios) {
      if (h_exist.hora == hh && h_exist.minuto == mm) {
        server.send(400, "text/plain", "Error: ese horario ya existe");
        return;
      }
    }

    // Si llegamos acá, es único
    horarios.push_back({hh, mm});
    guardarHorarios();
    server.send(200, "text/plain", "Horario agregado");
  });

  server.on("/horarios", []() {
    String res = "Horarios:\n";
    for (auto& h : horarios) {
      res += String(h.hora) + ":" + (h.minuto < 10 ? "0" : "") + String(h.minuto) + "\n";
    }
    server.send(200, "text/plain", res);
  });

  server.begin();
  Serial.println("Servidor web iniciado en puerto 80");
  Serial.print("IP local: ");
  Serial.println(WiFi.localIP());
}

// ----------------------
// Loop principal
// ----------------------
void loop() {
  mqttClient.loop();
  server.handleClient();

  timeClient.update();
  setTime(timeClient.getEpochTime());
  verificarHorarios();
}
