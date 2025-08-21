#pragma once
#include <fstream>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include "../Models/Project_path.h"

class Config
{
  public:
    Config()
    {
        reload();
    }

    // Функция reload() выполняет перезагрузку конфигурации из файла settings.json
    // Открывает файл, читает JSON-данные в объект config и закрывает файл
    // Используется при инициализации и для обновления настроек во время выполнения
    void reload()
    {
        std::ifstream fin(project_path + "settings.json");
        fin >> config;
        fin.close();
    }

    // Оператор () перегружен для удобного доступа к настройкам конфигурации
    // Позволяет обращаться к вложенным JSON-значениям через синтаксис config("раздел", "параметр")
    // Например: config("Bot", "IsWhiteBot") вместо config.config["Bot"]["IsWhiteBot"]
    // Возвращает значение настройки из JSON по указанному пути
    auto operator()(const string &setting_dir, const string &setting_name) const
    {
        return config[setting_dir][setting_name];
    }

  private:
    json config;
};
