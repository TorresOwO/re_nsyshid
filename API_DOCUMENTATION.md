# 📡 re_nsyshid REST API Documentation

This document describes the complete REST API specification implemented in the **`re_nsyshid`** Wii U plugin. It allows external applications (like **Skylanders Control**, Web Dashboards, Dolphin scripts, or Pokémon HOME style managers) to communicate directly with the Wii U over the local network.

---

## 🌐 General Information

- **Default Port:** `9090` (Configurable in the plugin menu `L + Down + SELECT`)
- **Base URL:** `http://<WII_U_IP>:9090`
- **Data Exchange Format:** `application/json` (or `application/octet-stream` for raw NFC dump downloads/uploads)
- **CORS:** Enabled (`Access-Control-Allow-Origin: *`, supports preflight `OPTIONS` requests)

---

## 📋 Endpoints Overview

| Category | Method | Endpoint | Description |
| :--- | :--- | :--- | :--- |
| **Status** | `GET` | `/api/skylanders/status` *(alias: `/api/status`)* | Get real-time portal status, loaded slots, live level & gold. |
| **Catalog** | `GET` | `/api/skylanders` | Get 670+ known figures with `game`, `element`, and `type`. |
| **Portal Control** | `POST` | `/api/skylanders/load` *(alias: `/api/load`)* | Place a Skylander or magic item on a portal slot. |
| **Portal Control** | `POST` | `/api/skylanders/remove` *(alias: `/api/remove`)* | Remove a Skylander from a specific slot. |
| **Portal Control** | `POST` | `/api/skylanders/clear` *(alias: `/api/clear`)* | Remove all figures from all slots. |
| **Vault / Storage** | `GET` | `/api/skylanders/vault` *(alias: `/api/skylanders/files`, `/api/vault`)* | List all `.sky` dumps stored on SD with decrypted stats & portal status. |
| **Vault / Storage** | `POST` | `/api/skylanders/delete` *(alias: `/api/delete`)* | Delete a `.sky` dump from SD card. |
| **Vault / Storage** | `POST` | `/api/skylanders/rename` *(alias: `/api/rename`)* | Rename a `.sky` dump on SD card. |
| **Vault / Storage** | `POST` | `/api/skylanders/duplicate` *(alias: `/api/duplicate`, `/api/skylanders/clone`)* | Clone / duplicate a `.sky` dump on SD card. |
| **Import / Export** | `GET` | `/api/skylanders/download` *(alias: `/api/download`)* | Export a 1024-byte `.sky` dump (from active slot or SD) to PC/browser. |
| **Import / Export** | `POST` | `/api/skylanders/upload` *(alias: `/api/upload`)* | Import a 1024-byte `.sky` dump from PC/browser to SD (binary or Base64 JSON). |
| **Web Dashboard** | `GET` | `/` or `/web` | Embedded single-page HTML/JS management dashboard. |

---

## 🚀 Detailed Endpoint Specifications

### 1. 📡 Status & Live Slots (`GET /api/skylanders/status`)

Returns the current emulation status and the 16 virtual slots of the portal, including **real-time AES-decrypted level and money** from active gameplay.

```http
GET /api/skylanders/status
Accept: application/json
```

#### Response (`200 OK`):
```json
{
  "success": true,
  "running": true,
  "message": "Connected to Dolphin Portal",
  "slots": [
    {
      "slot": 0,
      "portalSlot": 0,
      "occupied": true,
      "id": 16,
      "variant": 0,
      "name": "Spyro",
      "level": 12,
      "money": 4500
    },
    {
      "slot": 1,
      "portalSlot": 1,
      "occupied": true,
      "id": 101,
      "variant": 0,
      "name": "Healing Elixir",
      "level": 1,
      "money": 0
    },
    {
      "slot": 2,
      "portalSlot": -1,
      "occupied": false
    }
  ]
}
```

---

### 2. 📖 Catalog (`GET /api/skylanders`)

Returns all known Skylanders, Giants, Swappers, Trap Masters, Traps, Minis, Items, Vehicles, and Trophies with rich classification metadata.

```http
GET /api/skylanders
Accept: application/json
```

#### Response (`200 OK`):
```json
[
  {
    "id": 16,
    "variant": 0,
    "name": "Spyro",
    "game": "SSA",
    "element": "Magic",
    "type": "core"
  },
  {
    "id": 112,
    "variant": 4614,
    "name": "Tree Rex",
    "game": "SG",
    "element": "Life",
    "type": "giant"
  },
  {
    "id": 1000,
    "variant": 8192,
    "name": "Wash Buckler (Top)",
    "game": "SSF",
    "element": "Water",
    "type": "swapper"
  },
  {
    "id": 458,
    "variant": 12288,
    "name": "Wildfire",
    "game": "STT",
    "element": "Fire",
    "type": "trap-master"
  },
  {
    "id": 213,
    "variant": 12292,
    "name": "Undead Skull Trap",
    "game": "STT",
    "element": "Undead",
    "type": "trap"
  },
  {
    "id": 3224,
    "variant": 16384,
    "name": "Hot Streak",
    "game": "SSC",
    "element": "Fire",
    "type": "vehicle"
  }
]
```

**Classification Values:**
- **`game`**: `"SSA"`, `"SG"`, `"SSF"`, `"STT"`, `"SSC"`
- **`element`**: `"Magic"`, `"Water"`, `"Fire"`, `"Tech"`, `"Undead"`, `"Earth"`, `"Life"`, `"Air"`, `"Light"`, `"Dark"`, `"Kaos"`
- **`type`**: `"core"`, `"giant"`, `"swapper"`, `"trap-master"`, `"trap"`, `"mini"`, `"item"`, `"vehicle"`, `"trophy"`

---

### 3. ⚡ Load Figure onto Portal (`POST /api/skylanders/load`)

Places a figure into a virtual portal slot. If the `.sky` dump does not exist on the SD card, the server automatically generates a valid dump in `/vol/external01/wiiu/re_nsyshid/Skylanders/`.

```http
POST /api/skylanders/load
Content-Type: application/json

{
  "slot": 0,
  "name": "Spyro",
  "id": 16,
  "variant": 0
}
```

> **Optional Body Fields:**
> - `slot` *(number, default: 0)*: Destination slot (0 - 15).
> - `name` *(string, optional)*: Character name.
> - `id` *(number, optional)*: Numeric character ID.
> - `variant` *(number, optional)*: Variant / repose code.
> - `path` *(string, optional)*: Direct SD path to an existing `.sky` dump.

#### Response (`200 OK`):
```json
{
  "success": true,
  "slot": 0,
  "portalSlot": 0,
  "name": "Spyro",
  "message": "Skylander Spyro loaded successfully into slot 0"
}
```

---

### 4. 🗑️ Remove Figure from Portal (`POST /api/skylanders/remove`)

Removes the figure currently placed on the specified slot.

```http
POST /api/skylanders/remove
Content-Type: application/json

{
  "slot": 0
}
```

---

### 5. 🧹 Clear Entire Portal (`POST /api/skylanders/clear`)

Clears all 16 slots at once.

```http
POST /api/skylanders/clear
Content-Type: application/json

{}
```

---

### 6. 🏦 Vault / Storage Library (`GET /api/skylanders/vault`)

Scans all `.sky` dump files in the Wii U SD storage directory (`/vol/external01/wiiu/re_nsyshid/Skylanders/`), decrypts each figure's level and money, and reports whether it is currently loaded on the portal.

```http
GET /api/skylanders/vault
Accept: application/json
```

#### Response (`200 OK`):
```json
{
  "success": true,
  "count": 2,
  "figures": [
    {
      "filename": "Spyro.sky",
      "path": "/vol/external01/wiiu/re_nsyshid/Skylanders/Spyro.sky",
      "size": 1024,
      "id": 16,
      "variant": 0,
      "name": "Spyro",
      "game": "SSA",
      "element": "Magic",
      "type": "core",
      "level": 12,
      "money": 4500,
      "loaded": true,
      "slot": 0
    },
    {
      "filename": "TreeRex_Max.sky",
      "path": "/vol/external01/wiiu/re_nsyshid/Skylanders/TreeRex_Max.sky",
      "size": 1024,
      "id": 112,
      "variant": 4614,
      "name": "Tree Rex",
      "game": "SG",
      "element": "Life",
      "type": "giant",
      "level": 20,
      "money": 65000,
      "loaded": false
    }
  ]
}
```

---

### 7. 💾 Export / Download `.sky` Dump (`GET /api/skylanders/download`)

Exports a 1024-byte binary `.sky` dump compatible with Dolphin, RPCS3, Cemu, and Flipper Zero.

#### Download by active slot:
```http
GET /api/skylanders/download?slot=0
```

#### Download by filename on SD:
```http
GET /api/skylanders/download?filename=Spyro.sky
```

#### Download as JSON Base64 (optional):
```http
GET /api/skylanders/download?slot=0&format=json
```

---

### 8. 📤 Import / Upload `.sky` Dump (`POST /api/skylanders/upload`)

Imports any 1024-byte `.sky` / `.bin` dump to the Wii U SD card and optionally loads it onto the portal immediately.

#### Mode A: JSON with Base64
```http
POST /api/skylanders/upload
Content-Type: application/json

{
  "filename": "MyCustomSpyro.sky",
  "data": "<1024 bytes Base64 string>",
  "slot": 0,
  "load": true
}
```

#### Mode B: Raw Binary
```http
POST /api/skylanders/upload?filename=MyCustomSpyro.sky&slot=0
Content-Type: application/octet-stream

[1024 binary bytes]
```

#### Response (`200 OK`):
```json
{
  "success": true,
  "filename": "MyCustomSpyro.sky",
  "path": "/vol/external01/wiiu/re_nsyshid/Skylanders/MyCustomSpyro.sky",
  "size": 1024,
  "loaded": true,
  "slot": 0,
  "portalSlot": 0,
  "figureName": "Spyro",
  "message": "File uploaded successfully: MyCustomSpyro.sky"
}
```

---

### 9. 🗑️ Delete File from SD (`POST /api/skylanders/delete`)

```http
POST /api/skylanders/delete
Content-Type: application/json

{
  "filename": "OldFigure.sky"
}
```

---

### 10. ✏️ Rename File on SD (`POST /api/skylanders/rename`)

```http
POST /api/skylanders/rename
Content-Type: application/json

{
  "oldFilename": "Spyro.sky",
  "newFilename": "Spyro_Level20.sky"
}
```

---

### 11. 📑 Duplicate / Clone File on SD (`POST /api/skylanders/duplicate`)

```http
POST /api/skylanders/duplicate
Content-Type: application/json

{
  "filename": "Spyro.sky",
  "newFilename": "Spyro_Backup.sky"
}
```
