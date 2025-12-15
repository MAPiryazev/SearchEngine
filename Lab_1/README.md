# SearchEngine
MAI_Information_Search

#  ЛР1: Добыча корпуса документов

Этот репозиторий готовит корпус документов из двух источников (Русская Википедия + ru.stackoverflow.com) и приводит его к единому формату JSONL для дальнейших лабораторных работ.


## Источники данных

### 1) Русская Википедия (Wikimedia Dumps)

Источник: официальный сайт дампов Wikimedia Dumps. [web:31]  
Используемый дамп: `ruwiki-latest-pages-articles.xml.bz2` из каталога `ruwiki/latest/`. [web:33]  
Формат: XML-дамп со статьями и ревизиями; исходный текст хранится в `<revision><text>` и содержит wiki-разметку. [web:32][web:35]

### 2) Stack Overflow на русском (Stack Exchange Data Dump)

Источник: Stack Exchange Data Dump на Internet Archive. [web:36]  
Используемый архив: `ru.stackoverflow.com.7z`, внутри основной файл для контента — `Posts.xml`. [web:36][web:25]


## Из чего состоит текст и мета

### Википедия

- Сырые данные: XML + wiki-разметка в поле `<text>`. [web:32]
- Дополнительная мета: `title`, `id`, `timestamp` ревизии и т.п.
- Разметка: wiki-разметка (шаблоны `{{}}`, wiki-ссылки `[[...]]`, HTML-теги и т.д.). [web:35]
- Выделение текста: из `<text>` удаляется разметка (шаблоны, файлы/изображения, ссылки, HTML), нормализуются пробелы.

### Stack Overflow

- Сырые данные: XML `Posts.xml` из Stack Exchange dump. [web:25][web:27]
- Состав: посты разных типов; `PostTypeId="1"` — вопросы, `PostTypeId="2"` — ответы. [web:25][web:27]
- Мета: `Id`, `CreationDate`, `Tags`, `Score`, `ViewCount`, `AnswerCount` и т.д.
- Разметка: HTML в `Body` (в т.ч. блоки кода). [web:27]
- Выделение текста: HTML теги удаляются, блоки кода заменяются маркером, текст нормализуется.


## Итоговый формат документа (JSONL)

Каждая строка `docs.jsonl` — отдельный JSON-объект.

Обязательные поля:
- `id`: внутренний ID (0..N-1)
- `external_id`: ID в исходном источнике
- `source`: `"wiki"` или `"stackoverflow"`
- `title`: заголовок
- `text`: очищенный текст
- `meta`: мета-информация (например `url`, `timestamp/creation_date`, `raw_length_bytes`, `language`, а для StackOverflow ещё `tags`, `score`, `views`)


## Статистика корпуса (фактические значения)

### Википедия (ruwiki)
- Raw: `ruwiki-latest-pages-articles.xml.bz2` ≈ 5.5 GB.  
- Extracted: `corpus/extracted/wiki/docs.jsonl` ≈ 879 MB.  
- Количество документов: 50 000.

### Stack Overflow (ru.stackoverflow.com)
- Raw: `ru.stackoverflow.com.7z` ≈ 1.0 GB. [web:36]  
- Extracted: `corpus/extracted/stackexchange/docs.jsonl` ≈ 50 MB.  
- Количество документов: 50 000.


## Существующие поисковики (требование лабы)

Корпус **можно** использовать, потому что для обоих источников существует готовый поиск.

### Поиск по Википедии
- Встроенный поиск MediaWiki: `https://ru.wikipedia.org/wiki/Special:Search?search=<ЗАПРОС>` (Special:Search). [web:62]
- При необходимости есть API поиска MediaWiki (`action=query&list=search&srsearch=...`). [web:56]

Примеры запросов:
- `https://ru.wikipedia.org/wiki/Special:Search?search=машинное+обучение+python` [web:62]
- `https://ru.wikipedia.org/wiki/Special:Search?search=инвертированный+индекс` [web:62]

Недостатки выдачи (пример):
- Иногда в топе оказываются результаты из-за совпадения отдельных слов, а не по смыслу; требуется уточнять запрос (операторы/переформулировка). [web:62]

### Поиск по ru.stackoverflow.com
- Встроенный поиск и операторы: `https://ru.stackoverflow.com/search?q=<ЗАПРОС>`, поддерживаются поисковые операторы (например `is:question`). [web:54]
- Также можно использовать Google с ограничением на сайт (`site:ru.stackoverflow.com ...`); Stack Overflow прямо рекомендует использовать Google как альтернативу встроенному поиску. [web:58]

Примеры запросов:
- `https://ru.stackoverflow.com/search?q=python+smtp+вложением` [web:54]
- `https://ru.stackoverflow.com/search?q=is%3Aquestion+mongodb+docker+connection+refused` [web:54]
- Google: `site:ru.stackoverflow.com python smtp вложение` [web:58]

Недостатки выдачи (пример):
- В выдаче могут подниматься старые вопросы; без уточняющих операторов/фильтров сложнее получить “актуальные” решения. [web:54]

