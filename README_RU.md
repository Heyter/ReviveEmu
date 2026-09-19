# ReviveEmu

ReviveEmu — кроссплатформенный форк классического Steam.dll-эмулятора RevEmu. В этом форке также находится Linux-бэкенд `libsteamclient.so` для старого SteamClient API, используемый сервером **Counter-Strike: Source V34 / Build 4100**.

Для `server_v34` ReviveEmu является **единственной поддерживаемой точкой эмуляции/аутентификации**. eSTEAMATiON, REVOLUTiON и другие Steam-эмуляторы одновременно с ним использовать не нужно.

Английская документация: [README.md](README.md).

## Production-сборка / GitLab CI

`.gitlab-ci.yml` собирает production release только для ветки `prod`.

GitLab Runner:

1. устанавливает i386/multilib toolchain;
2. проверяет зафиксированные Build 4100 зависимости в `bin-deps/SHA256SUMS`;
3. собирает `libsteam.so`, legacy `libsteamclient.so` и validator как ELF32/i386;
4. запускает CTest;
5. добавляет необходимые runtime dynamic libraries из `bin-deps`;
6. раскладывает файлы сразу по структуре `server_v34`;
7. публикует готовую директорию и `.tar.gz` как GitLab artifacts.

Точка входа:

```sh
./releases/build-prod.sh
```

Результат:

```text
releases/
├── ReviveEmu-server_v34-prod/
│   ├── bin/
│   │   ├── libsteam.so
│   │   ├── steamclient_i486.so
│   │   ├── libsteamvalidateuseridtickets_i486.so
│   │   ├── libsteamvalidateuseridtickets_valve.so
│   │   ├── valve_api_i486.so
│   │   ├── steam_api_i486.so
│   │   └── rev.ini
│   ├── rev.ini
│   ├── cleanup-old-emulators.sh
│   ├── REVIVEEMU_README_RU.md
│   ├── BUILD_INFO.txt
│   └── SHA256SUMS
└── ReviveEmu-server_v34-prod.tar.gz
```

`.tar.gz` намеренно создаётся **без внешней папки-обёртки**. Его можно распаковать непосредственно в корень сервера:

```sh
tar -xzf ReviveEmu-server_v34-prod.tar.gz -C /path/to/server_v34
```

`bin-deps/` в runtime release не попадает. Все runtime-библиотеки из него (`*.so`, `*.so.*`, `*.dll`, `*.dylib`) копируются непосредственно в `server_v34/bin/`.

## `bin-deps`: бинарные зависимости Build 4100

В `ReviveEmu/bin-deps/` хранятся проверенные бинарные входные зависимости из принятого `server_v34` Build 4100:

```text
bin-deps/
├── libsteamvalidateuseridtickets_valve.so
├── valve_api_i486.so
├── SHA256SUMS
└── README.md
```

- `libsteamvalidateuseridtickets_valve.so` — оригинальный Valve validator Build 4100, сохранённый под отдельным именем для legacy `BSL::*` ABI.
- `valve_api_i486.so` — чистый оригинальный Valve Steam API shim.
- `SHA256SUMS` — фиксирует разрешённые бинарники и проверяется CI перед сборкой.

Старый `server_v34/bin/steamclient_i486.so` в `bin-deps` не входит: его заменяет собранный ReviveEmu backend. Старый `server_v34/bin/steam_api_i486.so` также не используется, потому что в исходном legacy server bundle он содержит eSTEAMATiON. Production builder создаёт чистый `bin/steam_api_i486.so` из `bin-deps/valve_api_i486.so`.

### Локальная production-сборка

Debian/Ubuntu:

```sh
apt-get update
apt-get install -y build-essential cmake ninja-build file git binutils patchelf gcc-multilib g++-multilib
./releases/build-prod.sh
```

## Установка и эксплуатация на `server_v34`

### 1. Распаковать release

Остановить сервер и распаковать archive прямо в корень `server_v34`:

```sh
tar -xzf ReviveEmu-server_v34-prod.tar.gz -C /path/to/server_v34
cd /path/to/server_v34
```

Ручное переименование библиотек больше не требуется: production builder уже использует реальные runtime-имена Build 4100.

### 2. Удалить конфликтующие старые эмуляторы

После распаковки выполнить:

```sh
chmod +x cleanup-old-emulators.sh
./cleanup-old-emulators.sh
```

Скрипт удаляет только конфликтующие legacy-файлы/configs, например:

```text
bin/steamclient.so
bin/libeST_SCI.so
bin/libeST_STEAM2.so
bin/libSteam2Auth.so
rev.cfg
*esteamation* configs
*revolution* configs
```

Он **не удаляет** активные файлы ReviveEmu `bin/steamclient_i486.so`, `bin/libsteamvalidateuseridtickets_i486.so`, `bin/libsteam.so` и `rev.ini`.

### 3. Проверить release

```sh
sha256sum -c SHA256SUMS
```


### 4. Настроить `rev.ini`

Release уже содержит рекомендуемую конфигурацию:

```ini
[Emulator]
CacheEnabled=False
Language=English
Logging=True
SteamUser=ReviveServer
CompatibilityMode=None
ForceRevClient=False

[Log]
FileSystem=False
Account=False
UserID=True
```

Для CS:S V34 Build 4100 **не добавлять `SteamDll=`**.

`rev.ini` находится и в корне сервера, и в `bin/`. Если меняете конфигурацию вручную, держите копии одинаковыми:

```sh
cp rev.ini bin/rev.ini
```

### 5. Запустить сервер

ReviveEmu **не является отдельным daemon/process**. Ничего вроде `./reviveemu` запускать не нужно. SRCDS сам загружает библиотеки из `bin/`.

Запускайте `server_v34` обычным штатным способом — например существующим `./start.sh`, service unit или container entrypoint вашей серверной сборки.

Для Linux legacy SteamClient backend рекомендуются:

```sh
REVIVE_STEAMCLIENT_LOG=./revive_steamclient.log
REVIVE_LOG_MODE=production
REVIVE_ABI_TRACE=0
```

`Logging=True` в `rev.ini` управляет классическим `rev.log`; `REVIVE_LOG_MODE` — отдельным логом Linux SteamClient backend.

### Обновление

1. остановить сервер;
2. при необходимости сохранить свой `rev.ini`;
3. распаковать новый archive поверх `server_v34`;
4. выполнить `./cleanup-old-emulators.sh`;
5. проверить `sha256sum -c SHA256SUMS`;
6. вернуть/изменить свой `rev.ini` и выполнить `cp rev.ini bin/rev.ini`;
7. запустить сервер штатным способом.

## Полное описание `rev.ini`

ReviveEmu сначала загружает `rev.ini` из директории своей библиотеки. Если файла там нет, используется `rev.ini` из текущей рабочей директории процесса.

### Секция `[Emulator]`

| Параметр | Значения / default | Что делает |
| --- | --- | --- |
| `CacheEnabled` | `True` / `False`, default `False` | Включает старую Steam GCF-файловую систему. Для распакованного `server_v34` оставлять `False`. |
| `CachePath` | путь к директории | Папка, где находятся `.gcf`. Используется только при `CacheEnabled=True`. Если путь пустой, GCF-режим фактически отключится даже при `CacheEnabled=True`. |
| `CDRPath` | путь к файлу; по умолчанию `cdr.bin` рядом с `rev.ini` | Необязательный raw CDR (Content Description Record) с описанием приложений/cache requirements. Нужен только для старого GCF mounting. |
| `Language` | `English`, `French`, `Italian`, `German`, `Spanish`, `sChinese`, `Korean`, `Koreana`, `tChinese`, `Japanese`, `Russian`, `Thai`, `Portuguese`; default `English` | Язык старого Steam-интерфейса и локализованных cache requirements. На Windows явно заданное значение в INI имеет приоритет над старым Steam Language из registry. |
| `Logging` | `True` / `False`, default `False` | Включает классический лог ReviveEmu `rev.log`. Параметры секции `[Log]` ниже добавляют конкретные группы сообщений. |
| `SteamDll` | путь к официальному legacy `Steam.dll`; по умолчанию не задан | Загружает официальный Steam DLL и позволяет проксировать legacy Steam UserID validation. **Для `server_v34` этот параметр должен отсутствовать.** |
| `SteamUser` | строка, default `RevUser` | Имя пользователя старого эмулируемого Steam. ReviveEmu также выставляет его в переменную окружения `SteamUser`. Для dedicated-сервера удобно `ReviveServer`. |
| `CompatibilityMode` | `None` или `2003`, default `None` | Включает поведение очень старого Steam.dll, необходимое некоторым древним GoldSource-сборкам. Для CS:S V34 — `None`. |
| `ForceRevClient` | `True` / `False`, фактический default `False` | При `True` неизвестные/non-Revive client tickets отклоняются вместо допуска с SteamID, вычисленным из IP. Это compatibility-переключатель классического validator path. В текущем `server_v34` используется `False`; строгая обработка ClassicRevEmu ticket в Linux legacy SteamClient backend реализована отдельно. |
| `SteamClient` | `True` / `False`, default `False`; только Windows | Разрешает загрузку соседнего `steamclient.dll`: ReviveEmu записывает старые registry-ключи `ActiveProcess`. В Linux `server_v34` не используется. |

### Секция `[Log]`

Эти параметры работают только при `[Emulator] Logging=True`.

| Параметр | Что делает |
| --- | --- |
| `FileSystem` | Добавляет подробный лог старых filesystem/GCF операций. Для обычного сервера с распакованными файлами обычно `False`. |
| `Account` | Добавляет лог старых account/subscription API вызовов. Для dedicated CS:S обычно не нужен. |
| `UserID` | Добавляет лог Steam UserID и ticket validation. Наиболее полезная категория классического validator при диагностике авторизации старого сервера. |

### Режим для диагностики авторизации

Временно можно использовать:

```ini
[Emulator]
Logging=True

[Log]
FileSystem=False
Account=False
UserID=True
```

После диагностики оставить только реально нужные логи. `rev.log` и `revive_steamclient.log` управляются независимо.

## Обычное использование ReviveEmu

### Распакованные файлы

Для старой игры, которая напрямую использует Steam.dll:

1. Положить подходящую ReviveEmu Steam-библиотеку рядом с executable игры; у Source Engine обычно используется `bin/`.
2. Положить `rev.ini` рядом с библиотекой ReviveEmu либо в рабочую директорию игры.
3. При необходимости задать AppID через `steam_appid.txt`, параметр `-appid <id>` или переменную окружения `SteamAppId`.
4. Запустить игру/сервер.

### GCF mounting

1. Установить `CacheEnabled=True`.
2. В `CachePath` указать папку с GCF.
3. Дать ReviveEmu один источник описания приложений/cache: `cdr.bin`, `ClientRegistry.blob` или настроенный `revApps.ini`.
4. Задать правильный Steam AppID.
5. Запустить игру/сервер.

## Ключевые правила для `server_v34`

- ReviveEmu — единственный emulator backend.
- Оригинальный `valve_api_i486.so` сохраняем.
- `steam_api_i486.so` перед запуском восстанавливаем из `valve_api_i486.so`.
- Оригинальный Valve validator сохраняем как `libsteamvalidateuseridtickets_valve.so`.
- ReviveEmu `libsteamclient.so` устанавливаем как `bin/steamclient_i486.so`.
- Пропатченный ReviveEmu validator устанавливаем как `bin/libsteamvalidateuseridtickets_i486.so`.
- Не оставляем одновременно `bin/steamclient.so`, eSTEAMATiON-библиотеки, REVOLUTiON или другой Steam emulator/config.
- Не задаём `SteamDll=`.
- `CacheEnabled=False`, пока сервер сознательно не переводится на GCF mounting.

## Credits

- shmelle, revCrew, vityan666, bir3yk — оригинальный RevEmu 2008 года.
- Bestest — Linux testing.
- Pancakes — macOS testing.
