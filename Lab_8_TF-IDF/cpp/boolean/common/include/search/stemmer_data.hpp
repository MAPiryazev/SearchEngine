#pragma once

#include <string>
#include <vector>
#include <utility>

namespace stemmer_data {

// Русские гласные для проверки основы
inline const std::u32string RU_VOWELS = U"аеиоуыэюя";

// Русские суффиксы для стемминга (в порядке от длинных к коротким)
inline const std::vector<std::u32string> RU_SUFFIXES = {
  // Творительный падеж множественного числа
  U"иями", U"ями", U"ами", U"ыми", U"ими",
  // Родительный падеж
  U"ого", U"его", U"ому", U"ему",
  // Прилагательные и причастия
  U"ее", U"ие", U"ые", U"ое", U"ая", U"яя", U"ую", U"юю", U"ою", U"ею",
  U"ей", U"ий", U"ый", U"ой",
  // Дательный падеж
  U"ам", U"ям", U"ом", U"ем", U"им", U"ым",
  // Предложный падеж
  U"ах", U"ях", U"ох", U"ех", U"их", U"ых",
  // Глагольные формы
  U"ать", U"ять", U"еть", U"ить", U"уть", U"ыть", U"оть",
  U"ает", U"яет", U"ет", U"ит", U"ут", U"ют", U"ат", U"ят",
  U"ал", U"ял", U"ел", U"ил", U"ул", U"ыл", U"ол",
  U"ала", U"яла", U"ела", U"ила", U"ула", U"ыла", U"ола",
  U"али", U"яли", U"ели", U"или", U"ули", U"ыли", U"оли",
  U"ан", U"ян", U"ен", U"ин", U"ун", U"ын", U"он",
  U"ана", U"яна", U"ена", U"ина", U"уна", U"ына", U"она",
  U"аны", U"яны", U"ены", U"ины", U"уны", U"ыны", U"оны",
  // Сравнительная степень
  U"ее", U"ей", U"е", U"ейше", U"ейш",
  // Существительные
  U"ость", U"есть", U"ация", U"ение", U"ание",
  // Падежные окончания
  U"а", U"я", U"ы", U"и", U"о", U"е", U"у", U"ю", U"ь"
};

// Английские правила Porter stemmer Step 2
inline const std::vector<std::pair<std::string, std::string>> EN_STEP2 = {
  {"ational", "ate"}, {"tional", "tion"}, {"enci", "ence"}, {"anci", "ance"},
  {"izer", "ize"}, {"abli", "able"}, {"alli", "al"}, {"entli", "ent"},
  {"eli", "e"}, {"ousli", "ous"}, {"ization", "ize"}, {"ation", "ate"},
  {"ator", "ate"}, {"alism", "al"}, {"iveness", "ive"}, {"fulness", "ful"},
  {"ousness", "ous"}, {"aliti", "al"}, {"iviti", "ive"}, {"biliti", "ble"}
};

// Английские правила Porter stemmer Step 3
inline const std::vector<std::pair<std::string, std::string>> EN_STEP3 = {
  {"icate", "ic"}, {"ative", ""}, {"alize", "al"}, {"iciti", "ic"},
  {"ical", "ic"}, {"ful", ""}, {"ness", ""}
};

// Английские суффиксы Porter stemmer Step 4
inline const std::vector<std::string> EN_STEP4 = {
  "al", "ance", "ence", "er", "ic", "able", "ible", "ant", "ement",
  "ment", "ent", "ion", "ou", "ism", "ate", "iti", "ous", "ive", "ize"
};

} // namespace stemmer_data

