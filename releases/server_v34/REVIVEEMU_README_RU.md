# ReviveEmu для CS:S V34 / Build 4100

Этот release уже разложен как корень `server_v34`. ReviveEmu не нужно запускать отдельным процессом: его `.so` загружаются самим SRCDS.

## Установка

1. Остановить сервер.
2. Распаковать архив **прямо в корень `server_v34`**:

   ```sh
   tar -xzf ReviveEmu-server_v34-prod.tar.gz -C /path/to/server_v34
   ```

3. Перейти в сервер и один раз удалить конфликтующие старые эмуляторы:

   ```sh
   cd /path/to/server_v34
   chmod +x cleanup-old-emulators.sh
   ./cleanup-old-emulators.sh
   ```

4. До изменения конфигов проверить целостность release-файлов:

   ```sh
   sha256sum -c SHA256SUMS
   ```

5. При необходимости отредактировать `rev.ini`. После изменения синхронизировать копию рядом с библиотеками:

   ```sh
   cp rev.ini bin/rev.ini
   ```

6. Запустить сервер обычным способом. ReviveEmu отдельно не запускается. Для готового runtime `server_v34` используется штатный запуск сервера (`./start.sh` либо ваш service/container entrypoint).

## Что куда устанавливается

```text
server_v34/
├── rev.ini
├── cleanup-old-emulators.sh
├── REVIVEEMU_README_RU.md
├── BUILD_INFO.txt
├── SHA256SUMS
└── bin/
    ├── libsteam.so
    ├── steamclient_i486.so
    ├── libsteamvalidateuseridtickets_i486.so
    ├── libsteamvalidateuseridtickets_valve.so
    ├── valve_api_i486.so
    ├── steam_api_i486.so
    └── rev.ini
```

`steamclient_i486.so`, `libsteamvalidateuseridtickets_i486.so` и `libsteam.so` — собранные ReviveEmu-библиотеки. `libsteamvalidateuseridtickets_valve.so` и `valve_api_i486.so` приходят из проверенного `bin-deps` Build 4100. `steam_api_i486.so` создаётся из чистого `valve_api_i486.so`, а не из старой eSTEAMATiON-сборки.

## `rev.ini` для server_v34

Рекомендуемый production-вариант уже лежит в release:

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

Для этого сервера не добавлять `SteamDll=`.

## Логи

Для legacy SteamClient backend используются переменные окружения:

```sh
REVIVE_STEAMCLIENT_LOG=./revive_steamclient.log
REVIVE_LOG_MODE=production
REVIVE_ABI_TRACE=0
```

`Logging=True` в `rev.ini` отвечает за классический `rev.log`. `REVIVE_LOG_MODE` управляет отдельным логом Linux SteamClient backend.

## Обновление ReviveEmu

Для обновления достаточно остановить сервер, распаковать новый production archive поверх корня `server_v34`, выполнить `./cleanup-old-emulators.sh`, проверить `sha256sum -c SHA256SUMS`, затем вернуть свои изменения `rev.ini` и запустить сервер. Свои изменения `rev.ini` перед обновлением при необходимости сохранить отдельно.
