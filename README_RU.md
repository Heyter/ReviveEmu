# ReviveEmu

ReviveEmu — Steam emulator/authentication backend для **Counter-Strike: Source V34 / Build 4100** в этом проекте. Поддерживаемая платформа развёртывания — Linux.

**Windows не поддерживается.**

Для `server_v34` ReviveEmu является **единственной поддерживаемой точкой эмуляции/авторизации**. Не нужно одновременно ставить eSTEAMATiON, REVOLUTiON или другой Steam emulator.

## Production-сборка / GitLab CI

`.gitlab-ci.yml` собирает production package в ветке `prod`.

GitLab Runner:

1. устанавливает Linux i386/multilib toolchain;
2. проверяет закреплённые Build 4100 зависимости через `bin-deps/SHA256SUMS`;
3. собирает `libsteam.so`, `libsteamclient.so` и `libsteamvalidateuseridtickets.so` как ELF32/i386;
4. запускает CTest;
5. копирует необходимые Linux `.so` из `bin-deps`;
6. формирует готовую структуру `server_v34`;
7. сохраняет пакет как временный GitLab job artifact и публикует `.tar.gz` вместе с checksum в **GitHub Releases**.


Собирается только Linux x86/i386. x64-сборки не создаются.

Чтобы GitLab Runner публиковал сборку в GitHub Releases, в GitLab → Settings → CI/CD → Variables нужно добавить:

- `GITHUB_TOKEN` — fine-grained GitHub token с `Contents: Read and write`; так как в репозитории изменяются workflow-файлы, также дать `Workflows: Read and write`. Переменную хранить как Masked/Hidden и, если `prod` защищена, Protected.
- `GITHUB_REPOSITORY` — целевой GitHub-репозиторий в формате `owner/repository`. По умолчанию используется GitLab `$CI_PROJECT_PATH`, поэтому при отличающемся GitHub owner/path значение нужно задать явно.

Release job создаёт GitHub Release с тегом `0.0.<CI_PIPELINE_IID>` и загружает:

```text
ReviveEmu-server_v34-prod.tar.gz
ReviveEmu-server_v34-prod.tar.gz.sha256
```

Точка входа сборки:

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

В `.tar.gz` нет внешней папки-обёртки, поэтому архив можно распаковать прямо в корень сервера:

```sh
tar -xzf ReviveEmu-server_v34-prod.tar.gz -C /path/to/server_v34
```

`bin-deps/` используется только как источник для сборки и отдельной папкой в release не попадает. Linux runtime-библиотеки (`*.so`, `*.so.*`) копируются непосредственно в `server_v34/bin/`.

## `bin-deps`

Для принятого Build 4100 используются:

```text
bin-deps/
├── libsteamvalidateuseridtickets_valve.so
├── valve_api_i486.so
├── SHA256SUMS
└── README.md
```

- `libsteamvalidateuseridtickets_valve.so` — оригинальный Valve validator Build 4100, сохранённый как ABI-зависимость для старых `BSL::*` symbols.
- `valve_api_i486.so` — чистый оригинальный Valve Steam API shim.
- `SHA256SUMS` закрепляет оба файла и проверяется перед production-сборкой.

Старый `server_v34/bin/steamclient_i486.so` не включается, потому что его заменяет ReviveEmu. Старый `server_v34/bin/steam_api_i486.so` также не используется, потому что в предоставленной серверной сборке он содержит eSTEAMATiON. Production builder создаёт чистый `steam_api_i486.so` из `valve_api_i486.so`.

## Локальная production-сборка

Debian/Ubuntu:

```sh
apt-get update
apt-get install -y build-essential cmake ninja-build file git binutils patchelf gcc-multilib g++-multilib
./releases/build-prod.sh
```

## Установка в `server_v34`

### 1. Остановить сервер и распаковать release

```sh
tar -xzf ReviveEmu-server_v34-prod.tar.gz -C /path/to/server_v34
cd /path/to/server_v34
```

Вручную переименовывать библиотеки не требуется. Production package уже содержит имена, которые ожидает Build 4100.

### 2. Удалить старые эмуляторы

```sh
chmod +x cleanup-old-emulators.sh
./cleanup-old-emulators.sh
```

Скрипт удалит конфликтующие библиотеки/конфиги старых эмуляторов и сохранит активный runtime ReviveEmu.

### 3. Проверить package

```sh
sha256sum -c SHA256SUMS
```

Проверку выполнять до редактирования `rev.ini`.

### 4. Настроить `rev.ini`

Рекомендуемый production-вариант:

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

В release лежат две копии `rev.ini`: в корне сервера и в `bin/`. После изменения синхронизировать их:

```sh
cp rev.ini bin/rev.ini
```

### 5. Запустить сервер

ReviveEmu не является отдельным процессом. Библиотеки загружает сам SRCDS из `bin/`.

Запускать сервер обычным способом, например:

```sh
./start.sh
```

Рекомендуемые переменные окружения legacy SteamClient backend:

```sh
REVIVE_STEAMCLIENT_LOG=./revive_steamclient.log
REVIVE_LOG_MODE=production
REVIVE_ABI_TRACE=0
```

## Обновление ReviveEmu

1. остановить сервер;
2. при необходимости сохранить свой `rev.ini`;
3. распаковать новый архив поверх `server_v34`;
4. выполнить `./cleanup-old-emulators.sh`;
5. до изменения release-файлов выполнить `sha256sum -c SHA256SUMS`;
6. вернуть/изменить `rev.ini` и скопировать его в `bin/rev.ini`;
7. запустить сервер штатным способом.

## Полное описание `rev.ini`

ReviveEmu сначала ищет `rev.ini` рядом со своей библиотекой, затем — в рабочей директории процесса.

### `[Emulator]`

| Параметр | Значения / default | Что делает |
| --- | --- | --- |
| `CacheEnabled` | `True` / `False`, default `False` | Включает старую Steam GCF-файловую систему. Для обычного `server_v34` с распакованными файлами оставлять `False`. |
| `CachePath` | путь к директории | Папка с `.gcf`. Используется только при `CacheEnabled=True`. |
| `CDRPath` | путь к файлу; default `cdr.bin` рядом с `rev.ini` | Необязательный raw Content Description Record для старых GCF/cache metadata. |
| `Language` | имя Steam-языка; default `English` | Выбирает язык старого Steam-интерфейса и локализованных cache/content requirements. |
| `Logging` | `True` / `False`, default `False` | Включает классический `rev.log`. Параметры секции `[Log]` включают дополнительные группы логирования. |
| `SteamUser` | строка, default `RevUser` | Имя эмулируемого legacy Steam-пользователя. Для dedicated server рекомендуется `ReviveServer`. |
| `CompatibilityMode` | `None` или `2003`, default `None` | Включает compatibility-поведение для очень старых сборок движка. Для CS:S V34 используется `None`. |
| `ForceRevClient` | `True` / `False`, default `False` | При включении отклоняет неизвестные/non-Revive tickets в классическом validator path. В текущем `server_v34` используется `False`; Linux legacy SteamClient backend отдельно проверяет ClassicRevEmu tickets. |

### `[Log]`

Эти параметры работают при `Logging=True`.

| Параметр | Что делает |
| --- | --- |
| `FileSystem` | Подробный лог старых filesystem/GCF операций. Для обычного `server_v34` обычно `False`. |
| `Account` | Лог legacy account/subscription API. Для dedicated server обычно не нужен. |
| `UserID` | Лог Steam UserID/ticket validation. Наиболее полезен при диагностике авторизации. |

### Логирование для диагностики

```ini
[Emulator]
Logging=True

[Log]
FileSystem=False
Account=False
UserID=True
```

`rev.log` и `revive_steamclient.log` — отдельные логи.

## Ключевые правила `server_v34`

- ReviveEmu — единственный emulator backend.
- Сохранять оригинальный `valve_api_i486.so`.
- Создавать `steam_api_i486.so` из чистого `valve_api_i486.so`.
- Оригинальный Valve validator сохранять как `libsteamvalidateuseridtickets_valve.so`.
- ReviveEmu `libsteamclient.so` устанавливается как `bin/steamclient_i486.so`.
- Пропатченный ReviveEmu validator устанавливается как `bin/libsteamvalidateuseridtickets_i486.so`.
- Не держать одновременно eSTEAMATiON, REVOLUTiON или другой Steam emulator.
- `CacheEnabled=False`, пока сервер сознательно не переводится на GCF mounting.

## Credits

- shmelle, revCrew, vityan666, bir3yk — оригинальный RevEmu.
- Bestest — Linux testing.
