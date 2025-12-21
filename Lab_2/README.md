
# Lab 2 — Search Engine Crawler

Поисковый робот для ЛР2 по курсу «Информационный поиск». Робот загружает документы из двух источников (Wikipedia и Stack Overflow) в MongoDB, поддерживает resume через чекпоинты и умеет переобкачивать документы с обновлением только при изменениях.


## Запуск MongoDB
```bash
docker-compose up -d
```

## Установка зависимостей
```bash
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt
```

## Запуск робота
Робот принимает единственный аргумент — путь до YAML-конфига.

```bash
python3 scripts/crawler.py config/crawler_config.yaml
```

Режимы:
```bash
python3 scripts/crawler.py config/crawler_config.yaml --mode seed
python3 scripts/crawler.py config/crawler_config.yaml --mode recrawl
python3 scripts/crawler.py config/crawler_config.yaml --mode auto
```

## Чекпоинты
Состояние сохраняется в `corpus/.crawler_state.json`. При повторном запуске seed-режим продолжает работу с места остановки.

## Тесты
Тесты лежат в папке tests
