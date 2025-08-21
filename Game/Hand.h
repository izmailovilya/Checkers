#pragma once
#include <tuple>

#include "../Models/Move.h"
#include "../Models/Response.h"
#include "Board.h"

// Класс для обработки пользовательского ввода (мышь, клавиатура)
class Hand
{
  public:
    // Конструктор принимает указатель на доску для взаимодействия с интерфейсом
    Hand(Board *board) : board(board)
    {
    }
    
    // Основная функция получения пользовательского ввода
    // Возвращает кортеж: (тип ответа, координата X клетки, координата Y клетки)
    tuple<Response, POS_T, POS_T> get_cell() const
    {
        SDL_Event windowEvent;
        Response resp = Response::OK;      // Изначально нейтральный ответ
        int x = -1, y = -1;               // Пиксельные координаты клика
        int xc = -1, yc = -1;             // Логические координаты клетки на доске
        
        // Основной цикл обработки событий
        while (true)
        {
            if (SDL_PollEvent(&windowEvent))  // Проверяем наличие событий
            {
                switch (windowEvent.type)
                {
                case SDL_QUIT:  // Пользователь закрыл окно
                    resp = Response::QUIT;
                    break;
                    
                case SDL_MOUSEBUTTONDOWN:  // Клик мыши
                    // Получаем пиксельные координаты клика
                    x = windowEvent.motion.x;
                    y = windowEvent.motion.y;
                    
                    // Преобразуем пиксельные координаты в логические координаты доски (0-7)
                    // Доска разделена на сетку 10x10, где 8x8 - игровое поле
                    xc = int(y / (board->H / 10) - 1);  // Строка (вертикальная координата)
                    yc = int(x / (board->W / 10) - 1);  // Столбец (горизонтальная координата)
                    
                    // Определяем тип клика по координатам
                    if (xc == -1 && yc == -1 && board->history_mtx.size() > 1)
                    {
                        // Клик в левом верхнем углу = кнопка "Назад" (если есть история ходов)
                        resp = Response::BACK;
                    }
                    else if (xc == -1 && yc == 8)
                    {
                        // Клик в правом верхнем углу = кнопка "Повтор игры"
                        resp = Response::REPLAY;
                    }
                    else if (xc >= 0 && xc < 8 && yc >= 0 && yc < 8)
                    {
                        // Клик в пределах игрового поля 8x8 = выбор клетки
                        resp = Response::CELL;
                    }
                    else
                    {
                        // Клик вне допустимых областей - игнорируем
                        xc = -1;
                        yc = -1;
                    }
                    break;
                    
                case SDL_WINDOWEVENT:  // События окна
                    if (windowEvent.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
                    {
                        // При изменении размера окна пересчитываем размеры доски
                        board->reset_window_size();
                        break;
                    }
                }
                
                // Если получили значимый ответ, выходим из цикла
                if (resp != Response::OK)
                    break;
            }
        }
        return {resp, xc, yc};  // Возвращаем результат
    }

    // Функция ожидания действия игрока (упрощенная версия get_cell)
    // Используется на финальном экране для ожидания решения о повторе игры
    Response wait() const
    {
        SDL_Event windowEvent;
        Response resp = Response::OK;
        
        while (true)
        {
            if (SDL_PollEvent(&windowEvent))
            {
                switch (windowEvent.type)
                {
                case SDL_QUIT:  // Закрытие окна
                    resp = Response::QUIT;
                    break;
                    
                case SDL_WINDOWEVENT_SIZE_CHANGED:  // Изменение размера окна
                    board->reset_window_size();
                    break;
                    
                case SDL_MOUSEBUTTONDOWN: {  // Клик мыши
                    int x = windowEvent.motion.x;
                    int y = windowEvent.motion.y;
                    int xc = int(y / (board->H / 10) - 1);
                    int yc = int(x / (board->W / 10) - 1);
                    
                    // Проверяем только клик на кнопку "Повтор игры"
                    if (xc == -1 && yc == 8)
                        resp = Response::REPLAY;
                }
                break;
                }
                
                if (resp != Response::OK)
                    break;
            }
        }
        return resp;
    }

  private:
    Board *board;
};
