# 🐾 Comedero Automático ESP8266

Este proyecto controla un **comedero automático** utilizando un ESP8266, conectado por WiFi y controlado por MQTT y una interfaz web. 

Usa un LED como simulación del mecanismo de apertura/cierre. Está pensado para alimentar mascotas automáticamente en horarios configurables.

## 🔧 Características

- 📡 Configuración WiFi con **WiFiManager**
- 🕐 Sincronización horaria con **NTP**
- ☁️ Comunicación MQTT (abrir, cerrar, configurar horarios)
- 🧠 Guardado de configuración y horarios en **LittleFS**
- 🌐 API HTTP para control remoto:
  - `/abrir` (opcional: `?tiempo=5`)
  - `/cerrar`
  - `/agregarHorario?hh=8&mm=0`
  - `/horarios`

## 📦 Requisitos

- Placa **ESP8266** (ej. NodeMCU, Wemos D1 mini)
- LED (simula apertura del comedero)
- Arduino IDE o PlatformIO
- Bibliotecas:
  - WiFiManager
  - PubSubClient
  - ESP8266WebServer
  - ArduinoJson
  - TimeLib
  - NTPClient
  - LittleFS

## 🧪 MQTT Topics

| Topic               | Descripción                    | Payload esperado         |
|---------------------|--------------------------------|--------------------------|
| `comedero/control`  | Abrir o cerrar comedero        | `"abrir"`, `"cerrar"`    |
| `comedero/horarios` | Enviar lista de nuevos horarios| JSON array (`hh`, `mm`)  |

### Ejemplo de payload horarios:

```json
{
  "horarios": [
    {"hh": 8, "mm": 0},
    {"hh": 20, "mm": 30}
  ]
}
```

## 📁 Archivos en LittleFS

- `/config.json`: configuración MQTT
- `/horarios.json`: lista de horarios programados

## 🛠 Configuración WiFi y MQTT

Si no hay configuración previa, se abre un portal WiFi llamado `ComederoSetup`. Ahí podés configurar:

- SSID y contraseña WiFi
- Host y puerto MQTT
- Client ID MQTT

## 🚀 Subida a GitHub

### Pasos rápidos:
```bash
git init
git add .
git commit -m "Primer commit - comedero automático"
git remote add origin https://github.com/usuario/repositorio.git
git push -u origin main
```

> 🔁 Reemplazá `usuario/repositorio.git` por el nombre real de tu repo

## 👨‍💻 Autor

Desarrollado por [Tu Nombre] – Proyecto ESP8266 para control de comedero automático por red.
