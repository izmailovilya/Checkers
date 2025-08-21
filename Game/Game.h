#pragma once
#include <chrono>
#include <thread>
#include <fstream>

#include "../Models/Project_path.h"
#include "../Models/Response.h"
#include "../Models/Move.h"
#include "Board.h"
#include "Config.h"
#include "Hand.h"
#include "Logic.h"

using namespace std;

class Game
{
  public:
    Game() : board(config("WindowSize", "Width"), config("WindowSize", "Hight")), hand(&board), logic(&board, &config)
    {
        ofstream fout(project_path + "log.txt", ios_base::trunc);
        fout.close();
    }

    // Главная функция игры - запускает игровой цикл шашек
    // Возвращает результат игры: 0 - ничья, 1 - победа белых, 2 - победа черных
    int play()
    {
        // Засекаем время начала игры для статистики
        auto start = chrono::steady_clock::now();
        
        // Обработка режима повтора игры
        if (is_replay)
        {
            logic = Logic(&board, &config);  // Пересоздаем логику игры
            config.reload();                 // Перезагружаем конфигурацию
            board.redraw();                  // Перерисовываем доску
        }
        else
        {
            board.start_draw();              // Первоначальная отрисовка доски
        }
        is_replay = false;

        // Инициализация игрового цикла
        int turn_num = -1;                   // Счетчик ходов (-1, чтобы первый ход был 0)
        bool is_quit = false;                // Флаг выхода из игры
        const int Max_turns = config("Game", "MaxNumTurns");  // Максимальное количество ходов
        
        // Основной игровой цикл
        while (++turn_num < Max_turns)
        {
            beat_series = 0;                 // Сброс счетчика серии взятий
            
            // Определяем возможные ходы для текущего игрока (turn_num % 2: 0=белые, 1=черные)
            logic.find_turns(turn_num % 2);
            
            // Если нет доступных ходов - игра окончена
            if (logic.turns.empty())
                break;
            
            // Устанавливаем глубину поиска для бота в зависимости от цвета
            logic.Max_depth = config("Bot", string((turn_num % 2) ? "Black" : "White") + string("BotLevel"));
            
            // Проверяем, играет ли человек или бот за текущий цвет
            if (!config("Bot", string("Is") + string((turn_num % 2) ? "Black" : "White") + string("Bot")))
            {
                // Ход человека
                auto resp = player_turn(turn_num % 2);
                
                // Обработка ответа игрока
                if (resp == Response::QUIT)
                {
                    is_quit = true;
                    break;
                }
                else if (resp == Response::REPLAY)
                {
                    is_replay = true;
                    break;
                }
                else if (resp == Response::BACK)  // Откат хода назад
                {
                    // Если предыдущий ход делал бот и это не серия взятий
                    if (config("Bot", string("Is") + string((1 - turn_num % 2) ? "Black" : "White") + string("Bot")) &&
                        !beat_series && board.history_mtx.size() > 2)
                    {
                        board.rollback();    // Откатываем ход бота
                        --turn_num;
                    }
                    if (!beat_series)
                        --turn_num;
                    
                    board.rollback();        // Откатываем ход игрока
                    --turn_num;
                    beat_series = 0;
                }
            }
            else
            {
                // Ход бота
                bot_turn(turn_num % 2);
            }
        }
        
        // Записываем время игры в лог
        auto end = chrono::steady_clock::now();
        ofstream fout(project_path + "log.txt", ios_base::app);
        fout << "Game time: " << (int)chrono::duration<double, milli>(end - start).count() << " millisec\n";
        fout.close();

        // Рекурсивный вызов для повтора игры
        if (is_replay)
            return play();
            
        // Выход из игры
        if (is_quit)
            return 0;
            
        // Определение результата игры
        int res = 2;  // По умолчанию победа черных
        if (turn_num == Max_turns)
        {
            res = 0;  // Ничья при достижении лимита ходов
        }
        else if (turn_num % 2)
        {
            res = 1;  // Победа белых (если последний ход был черных, но у них нет ходов)
        }
        
        // Показываем финальный экран и ждем реакции игрока
        board.show_final(res);
        auto resp = hand.wait();
        if (resp == Response::REPLAY)
        {
            is_replay = true;
            return play();  // Запускаем новую игру
        }
        return res;
    }

  private:
    // Выполняет ход бота (искусственного интеллекта)
    // Параметр color: цвет бота (false = белые, true = черные)
    void bot_turn(const bool color)
    {
        // Засекаем время начала хода бота для статистики
        auto start = chrono::steady_clock::now();

        // Получаем настроенную задержку для ходов бота
        auto delay_ms = config("Bot", "BotDelayMS");
        
        // Создаем параллельный поток для задержки, чтобы обеспечить минимальное время хода
        // Пока поток ждет, основной поток вычисляет лучший ход
        thread th(SDL_Delay, delay_ms);
        
        // Находим оптимальную последовательность ходов с помощью алгоритма ИИ
        auto turns = logic.find_best_turns(color);
        
        // Ждем завершения минимальной задержки
        th.join();
        
        bool is_first = true;
        
        // Выполняем все ходы в найденной последовательности (обычно это серия взятий)
        for (auto turn : turns)
        {
            // Добавляем задержку между ходами в серии (кроме первого)
            if (!is_first)
            {
                SDL_Delay(delay_ms);
            }
            is_first = false;
            
            // Увеличиваем счетчик взятий, если это ход со взятием (xb != -1)
            beat_series += (turn.xb != -1);
            
            // Выполняем ход на доске с анимацией
            board.move_piece(turn, beat_series);
        }

        // Записываем время выполнения хода бота в лог для анализа производительности
        auto end = chrono::steady_clock::now();
        ofstream fout(project_path + "log.txt", ios_base::app);
        fout << "Bot turn time: " << (int)chrono::duration<double, milli>(end - start).count() << " millisec\n";
        fout.close();
    }

    // Обрабатывает ход игрока (не бота)
    // Параметр color: цвет игрока (false = белые, true = черные)
    // Возвращает Response - результат хода (OK, QUIT, REPLAY, BACK)
    Response player_turn(const bool color)
    {
        // Подготовка к первому ходу - сбор всех возможных начальных позиций
        vector<pair<POS_T, POS_T>> cells;
        for (auto turn : logic.turns)
        {
            cells.emplace_back(turn.x, turn.y);  // Добавляем все клетки с фигурами, которые могут ходить
        }
        board.highlight_cells(cells);  // Подсвечиваем все доступные для хода фигуры
        
        move_pos pos = {-1, -1, -1, -1};  // Инициализируем структуру хода
        POS_T x = -1, y = -1;             // Координаты выбранной фигуры
        
        // Цикл выбора фигуры и целевой клетки для хода
        while (true)
        {
            auto resp = hand.get_cell();  // Получаем ответ от пользователя (клик мыши)
            
            // Если игрок нажал кнопку (не клетку), возвращаем соответствующий ответ
            if (get<0>(resp) != Response::CELL)
                return get<0>(resp);
                
            pair<POS_T, POS_T> cell{get<1>(resp), get<2>(resp)};  // Координаты кликнутой клетки

            bool is_correct = false;
            
            // Проверяем, корректна ли кликнутая клетка
            for (auto turn : logic.turns)
            {
                // Проверяем, является ли клетка начальной позицией для возможного хода
                if (turn.x == cell.first && turn.y == cell.second)
                {
                    is_correct = true;
                    break;
                }
                // Проверяем, является ли клетка целевой для уже выбранной фигуры
                if (turn == move_pos{x, y, cell.first, cell.second})
                {
                    pos = turn;  // Сохраняем полный ход
                    break;
                }
            }
            
            // Если найден полный ход, выходим из цикла
            if (pos.x != -1)
                break;
                
            // Если клетка некорректна, сбрасываем выделение
            if (!is_correct)
            {
                if (x != -1)
                {
                    board.clear_active();      // Убираем выделение активной фигуры
                    board.clear_highlight();   // Убираем подсветку возможных ходов
                    board.highlight_cells(cells);  // Восстанавливаем исходную подсветку
                }
                x = -1;
                y = -1;
                continue;
            }
            
            // Выбираем фигуру и показываем возможные ходы для неё
            x = cell.first;
            y = cell.second;
            board.clear_highlight();
            board.set_active(x, y);  // Выделяем выбранную фигуру
            
            // Собираем все возможные целевые клетки для выбранной фигуры
            vector<pair<POS_T, POS_T>> cells2;
            for (auto turn : logic.turns)
            {
                if (turn.x == x && turn.y == y)
                {
                    cells2.emplace_back(turn.x2, turn.y2);  // Добавляем целевые позиции
                }
            }
            board.highlight_cells(cells2);  // Подсвечиваем возможные ходы
        }
        
        // Выполняем первый ход
        board.clear_highlight();
        board.clear_active();
        board.move_piece(pos, pos.xb != -1);  // Перемещаем фигуру (pos.xb != -1 означает взятие)
        
        // Если это не взятие, ход завершен
        if (pos.xb == -1)
            return Response::OK;
            
        // Обработка серии взятий (если фигура может продолжить бить)
        beat_series = 1;
        while (true)
        {
            // Проверяем, может ли фигура продолжить бить с новой позиции
            logic.find_turns(pos.x2, pos.y2);
            if (!logic.have_beats)  // Если больше нет возможности бить
                break;

            // Собираем возможные целевые клетки для продолжения взятий
            vector<pair<POS_T, POS_T>> cells;
            for (auto turn : logic.turns)
            {
                cells.emplace_back(turn.x2, turn.y2);
            }
            board.highlight_cells(cells);        // Подсвечиваем возможные ходы
            board.set_active(pos.x2, pos.y2);    // Выделяем фигуру на новой позиции
            
            // Цикл выбора следующего хода в серии взятий
            while (true)
            {
                auto resp = hand.get_cell();
                if (get<0>(resp) != Response::CELL)
                    return get<0>(resp);
                pair<POS_T, POS_T> cell{get<1>(resp), get<2>(resp)};

                // Проверяем корректность выбранной целевой клетки
                bool is_correct = false;
                for (auto turn : logic.turns)
                {
                    if (turn.x2 == cell.first && turn.y2 == cell.second)
                    {
                        is_correct = true;
                        pos = turn;  // Сохраняем ход
                        break;
                    }
                }
                if (!is_correct)
                    continue;

                // Выполняем очередной ход в серии взятий
                board.clear_highlight();
                board.clear_active();
                beat_series += 1;  // Увеличиваем счетчик взятий в серии
                board.move_piece(pos, beat_series);
                break;
            }
        }

        return Response::OK;  // Ход успешно завершен
    }

  private:
    Config config;
    Board board;
    Hand hand;
    Logic logic;
    int beat_series;
    bool is_replay = false;
};
