#pragma once
#include <random>
#include <vector>

#include "../Models/Move.h"
#include "Board.h"
#include "Config.h"

const int INF = 1e9;

class Logic
{
  public:
    Logic(Board *board, Config *config) : board(board), config(config)
    {
        rand_eng = std::default_random_engine (
            !((*config)("Bot", "NoRandom")) ? unsigned(time(0)) : 0);
        scoring_mode = (*config)("Bot", "BotScoringType");
        optimization = (*config)("Bot", "Optimization");
    }

    // Главная функция поиска лучшей последовательности ходов для бота
    // Использует алгоритм минимакс для определения оптимальной стратегии
    // Параметр color: цвет бота (false = белые, true = черные)
    // Возвращает вектор ходов, которые следует выполнить (обычно серия взятий)
    vector<move_pos> find_best_turns(const bool color)
    {
        // Очищаем структуры данных для отслеживания лучших ходов
        next_best_state.clear();  // Массив ссылок на следующие состояния
        next_move.clear();        // Массив лучших ходов для каждого состояния
        
        // Запускаем поиск лучшего первого хода с текущего состояния доски
        // Начинаем с состояния 0, без предыдущих ходов (-1, -1)
        find_first_best_turn(board->get_board(), color, -1, -1, 0);
        
        // Восстанавливаем последовательность лучших ходов по цепочке состояний
        int cur_state = 0;           // Начинаем с начального состояния
        vector<move_pos> result;     // Результирующая последовательность ходов
        
        // Проходим по цепочке лучших состояний до конца
        do {
            result.push_back(next_move[cur_state]);      // Добавляем ход из текущего состояния
            cur_state = next_best_state[cur_state];      // Переходим к следующему состоянию
        } while (cur_state != -1 && next_move[cur_state].x != -1);  // Пока не дойдем до конца цепочки
        
        return result;
    }

private:
    // Выполняет ход на копии доски и возвращает новое состояние
    // Параметр mtx: текущее состояние доски
    // Параметр turn: ход для выполнения
    vector<vector<POS_T>> make_turn(vector<vector<POS_T>> mtx, move_pos turn) const
    {
        // Удаляем побитую фигуру (если есть взятие)
        if (turn.xb != -1)
            mtx[turn.xb][turn.yb] = 0;
            
        // Превращение в дамку при достижении противоположного края
        if ((mtx[turn.x][turn.y] == 1 && turn.x2 == 0) || (mtx[turn.x][turn.y] == 2 && turn.x2 == 7))
            mtx[turn.x][turn.y] += 2;  // 1->3 (белая дамка), 2->4 (черная дамка)
            
        // Перемещаем фигуру с начальной позиции на конечную
        mtx[turn.x2][turn.y2] = mtx[turn.x][turn.y];
        mtx[turn.x][turn.y] = 0;  // Очищаем начальную позицию
        
        return mtx;
    }

    // Вычисляет оценку позиции на доске для алгоритма минимакс
    // Параметр mtx: состояние доски
    // Параметр first_bot_color: цвет бота для которого максимизируем оценку
    double calc_score(const vector<vector<POS_T>> &mtx, const bool first_bot_color) const
    {
        // Подсчитываем материальное преимущество и позиционные факторы
        double w = 0, wq = 0, b = 0, bq = 0;
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                w += (mtx[i][j] == 1);
                wq += (mtx[i][j] == 3);
                b += (mtx[i][j] == 2);
                bq += (mtx[i][j] == 4);
                if (scoring_mode == "NumberAndPotential")
                {
                    w += 0.05 * (mtx[i][j] == 1) * (7 - i);
                    b += 0.05 * (mtx[i][j] == 2) * (i);
                }
            }
        }
        if (!first_bot_color)
        {
            swap(b, w);
            swap(bq, wq);
        }
        if (w + wq == 0)
            return INF;
        if (b + bq == 0)
            return 0;
        int q_coef = 4;
        if (scoring_mode == "NumberAndPotential")
        {
            q_coef = 5;
        }
        return (b + bq * q_coef) / (w + wq * q_coef);
    }

    // Специальная функция для обработки серий взятий (множественных ходов одной фигуры)
    // В шашках, если фигура может продолжить бить после первого взятия, она обязана это сделать
    // Параметры:
    //   mtx: текущее состояние доски
    //   color: цвет играющей стороны
    //   x, y: координаты фигуры (-1,-1 для первого хода)
    //   state: индекс текущего состояния в массивах next_move/next_best_state
    //   alpha: альфа-значение для отсечения (по умолчанию -1)
    double find_first_best_turn(vector<vector<POS_T>> mtx, const bool color, const POS_T x, const POS_T y, size_t state, double alpha = -1)
    {
        // Добавляем новое состояние в структуры отслеживания
        next_best_state.push_back(-1);           // Изначально нет следующего состояния
        next_move.emplace_back(-1, -1, -1, -1);  // Изначально нет хода
        
        double best_score = -1;  // Лучший найденный счет (для максимизирующего игрока)
        
        // Если это не первый ход в серии, ищем продолжение взятий с конкретной позиции
        if (state != 0) {
            find_turns(x, y, mtx);
        }
        
        // Сохраняем текущие найденные ходы
        auto turns_now = turns;
        bool have_beats_now = have_beats;
        
        // Если нет взятий и это не первый ход в серии, передаем ход противнику
        if (!have_beats_now && state != 0) {
            return find_best_turns_rec(mtx, 1 - color, 0, alpha);
        }
        
        // Перебираем все возможные ходы
        for (auto turn : turns_now) {
            size_t next_state = next_move.size();  // Индекс следующего состояния
            double score;
            
            if (have_beats_now) {
                // Если есть взятия, рекурсивно ищем продолжение серии взятий
                score = find_first_best_turn(make_turn(mtx, turn), color, turn.x2, turn.y2, next_state, best_score);
            } else {
                // Если нет взятий, переходим к обычному алгоритму минимакс
                score = find_best_turns_rec(make_turn(mtx, turn), 1 - color, 0, best_score);
            }
            
            // Обновляем лучший ход, если найден лучший счет
            if (score > best_score) {
                best_score = score;
                // Сохраняем ссылку на следующее состояние (если есть продолжение взятий)
                next_best_state[state] = (have_beats_now ? int(next_state) : -1);
                next_move[state] = turn;  // Сохраняем лучший ход
            }
        }
        
        return best_score;
    }

    // Основной рекурсивный алгоритм минимакс с альфа-бета отсечением
    // Реализует игровое дерево для поиска оптимального хода на заданную глубину
    // Параметры:
    //   mtx: текущее состояние доски
    //   color: цвет текущего игрока (0=белые, 1=черные)
    //   depth: текущая глубина поиска
    //   alpha: лучший счет для максимизирующего игрока (альфа-отсечение)
    //   beta: лучший счет для минимизирующего игрока (бета-отсечение)
    //   x, y: координаты конкретной фигуры для продолжения серии взятий (-1,-1 для обычного хода)
    double find_best_turns_rec(vector<vector<POS_T>> mtx, const bool color, const size_t depth, double alpha = -1, double beta = INF + 1, const POS_T x = -1, const POS_T y = -1)
    {
        // Условие остановки рекурсии: достигнута максимальная глубина поиска
        if (depth == Max_depth) {
            // Возвращаем оценку позиции: четные глубины - для начального игрока, нечетные - для противника
            return calc_score(mtx, (depth % 2 == color));
        }
        
        // Определяем возможные ходы
        if (x != -1) {
            // Если заданы конкретные координаты, ищем продолжение серии взятий
            find_turns(x, y, mtx);
        } else {
            // Иначе ищем все возможные ходы для данного цвета
            find_turns(color, mtx);
        }
        
        // Сохраняем результаты поиска ходов
        auto turns_now = turns;
        bool have_beats_now = have_beats;
        
        // Если нет взятий и мы продолжаем серию взятий, передаем ход противнику
        if (!have_beats_now && x != -1) {
            return find_best_turns_rec(mtx, 1 - color, depth + 1, alpha, beta);
        }
        
        // Если нет доступных ходов, игра окончена
        if (turns_now.empty()) {
            // Четные глубины - поражение текущего игрока, нечетные - победа
            return (depth % 2 ? 0 : INF);
        }
        
        // Инициализация значений для минимакс алгоритма
        double min_score = INF + 1;  // Лучший счет для минимизирующего игрока
        double max_score = -1;       // Лучший счет для максимизирующего игрока
        
        // Перебираем все возможные ходы
        for (auto turn : turns_now) {
            double score = 0.0;
            
            if (!have_beats_now && x == -1) {
                // Обычный ход - передаем ход противнику с увеличением глубины
                score = find_best_turns_rec(make_turn(mtx, turn), 1 - color, depth + 1, alpha, beta);
            } else {
                // Продолжение серии взятий - остается тот же игрок, глубина не увеличивается
                score = find_best_turns_rec(make_turn(mtx, turn), color, depth, alpha, beta, turn.x2, turn.y2);
            }
            
            // Обновляем минимальные и максимальные значения
            min_score = min(min_score, score);
            max_score = max(max_score, score);
            
            // Альфа-бета отсечение для оптимизации
            if (depth % 2) {
                // На нечетных глубинах максимизируем
                alpha = max(alpha, max_score);
            } else {
                // На четных глубинах минимизируем
                beta = min(beta, min_score);
            }
            
            // Отсечение: если alpha >= beta, дальнейший поиск бессмысленен
            if (optimization != "O0" && alpha >= beta) {
                // Возвращаем чуть лучший/худший счет для корректности отсечения
                return (depth % 2 ? max_score + 1 : min_score - 1);
            }
        }
        
        // Возвращаем результат в зависимости от четности глубины
        // Четные глубины - минимизируем, нечетные - максимизируем
        return (depth % 2 ? max_score : min_score);
    }

public:
    // Перегруженная функция поиска ходов для определенного цвета на текущей доске
    // Параметр color: цвет фигур для поиска (false = белые, true = черные)
    void find_turns(const bool color)
    {
        find_turns(color, board->get_board());
    }

    // Перегруженная функция поиска ходов для конкретной фигуры на текущей доске
    // Параметры x, y: координаты фигуры на доске
    void find_turns(const POS_T x, const POS_T y)
    {
        find_turns(x, y, board->get_board());
    }

private:
    // Основная функция поиска всех доступных ходов для определенного цвета
    // Параметр color: цвет фигур (false = белые, true = черные)
    // Параметр mtx: состояние доски в виде матрицы
    void find_turns(const bool color, const vector<vector<POS_T>> &mtx)
    {
        vector<move_pos> res_turns;
        bool have_beats_before = false;  // Флаг наличия взятий
        
        // Проходим по всем клеткам доски 8x8
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                // Проверяем, есть ли на клетке фигура нужного цвета
                if (mtx[i][j] && mtx[i][j] % 2 != color)
                {
                    // Ищем возможные ходы для этой фигуры
                    find_turns(i, j, mtx);
                    
                    // Если найдены взятия, очищаем список обычных ходов
                    if (have_beats && !have_beats_before)
                    {
                        have_beats_before = true;
                        res_turns.clear();  // В шашках взятия обязательны
                    }
                    
                    // Добавляем найденные ходы в результат
                    if ((have_beats_before && have_beats) || !have_beats_before)
                    {
                        res_turns.insert(res_turns.end(), turns.begin(), turns.end());
                    }
                }
            }
        }
        turns = res_turns;
        
        // Перемешиваем ходы для случайности (если включено в настройках)
        shuffle(turns.begin(), turns.end(), rand_eng);
        have_beats = have_beats_before;
    }

    // Функция поиска всех возможных ходов для конкретной фигуры
    // Параметры x, y: координаты фигуры на доске
    // Параметр mtx: состояние доски в виде матрицы
    void find_turns(const POS_T x, const POS_T y, const vector<vector<POS_T>> &mtx)
    {
        turns.clear();          // Очищаем предыдущие результаты
        have_beats = false;     // Сброс флага взятий
        POS_T type = mtx[x][y]; // Тип фигуры (1=белая шашка, 2=черная шашка, 3=белая дамка, 4=черная дамка)
        
        // Сначала проверяем возможности взятия (приоритетны в шашках)
        switch (type)
        {
        case 1:
        case 2:
            // check pieces
            for (POS_T i = x - 2; i <= x + 2; i += 4)
            {
                for (POS_T j = y - 2; j <= y + 2; j += 4)
                {
                    if (i < 0 || i > 7 || j < 0 || j > 7)
                        continue;
                    POS_T xb = (x + i) / 2, yb = (y + j) / 2;
                    if (mtx[i][j] || !mtx[xb][yb] || mtx[xb][yb] % 2 == type % 2)
                        continue;
                    turns.emplace_back(x, y, i, j, xb, yb);
                }
            }
            break;
        default:
            // check queens
            for (POS_T i = -1; i <= 1; i += 2)
            {
                for (POS_T j = -1; j <= 1; j += 2)
                {
                    POS_T xb = -1, yb = -1;
                    for (POS_T i2 = x + i, j2 = y + j; i2 != 8 && j2 != 8 && i2 != -1 && j2 != -1; i2 += i, j2 += j)
                    {
                        if (mtx[i2][j2])
                        {
                            if (mtx[i2][j2] % 2 == type % 2 || (mtx[i2][j2] % 2 != type % 2 && xb != -1))
                            {
                                break;
                            }
                            xb = i2;
                            yb = j2;
                        }
                        if (xb != -1 && xb != i2)
                        {
                            turns.emplace_back(x, y, i2, j2, xb, yb);
                        }
                    }
                }
            }
            break;
        }
        // check other turns
        if (!turns.empty())
        {
            have_beats = true;
            return;
        }
        switch (type)
        {
        case 1:
        case 2:
            // check pieces
            {
                POS_T i = ((type % 2) ? x - 1 : x + 1);
                for (POS_T j = y - 1; j <= y + 1; j += 2)
                {
                    if (i < 0 || i > 7 || j < 0 || j > 7 || mtx[i][j])
                        continue;
                    turns.emplace_back(x, y, i, j);
                }
                break;
            }
        default:
            // check queens
            for (POS_T i = -1; i <= 1; i += 2)
            {
                for (POS_T j = -1; j <= 1; j += 2)
                {
                    for (POS_T i2 = x + i, j2 = y + j; i2 != 8 && j2 != 8 && i2 != -1 && j2 != -1; i2 += i, j2 += j)
                    {
                        if (mtx[i2][j2])
                            break;
                        turns.emplace_back(x, y, i2, j2);
                    }
                }
            }
            break;
        }
    }

  public:
    vector<move_pos> turns;    // Список всех найденных возможных ходов
    bool have_beats;           // Флаг наличия обязательных взятий среди ходов
    int Max_depth;             // Максимальная глубина поиска для алгоритма минимакс

  private:
    default_random_engine rand_eng;    // Генератор случайных чисел для перемешивания ходов
    string scoring_mode;               // Режим оценки позиции ("NumberAndPotential" и др.)
    string optimization;               // Уровень оптимизации алгоритма (O0, O1, O2, O3)
    vector<move_pos> next_move;        // Массив лучших ходов для каждого состояния
    vector<int> next_best_state;       // Массив ссылок на следующие лучшие состояния
    Board *board;                      // Указатель на игровую доску
    Config *config;                    // Указатель на конфигурацию игры
};
