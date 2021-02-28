#ifndef THR_H
#define THR_H

#include <stdint.h>
#include <string.h>
#include <semaphore.h>
#include <pthread.h>

#include <vector>


//==============================================================================

typedef uint32_t t_sortint;

static_assert(sizeof(int) >= 4 && sizeof(size_t) >= 8, "sizeof");

//==============================================================================

class t_thr;

/* Базовый класс для задачи, которая может быть передана на выполнение треду.
 * Каждый объект задачи имеет свой task_id_, последовательный, начиная с 0. Ставится в конструкторе.
 * Это индекс в массиве t_vec_tasks, по которому тред будет выполнять задачу.
 *
 * Для отправки в тред, через fifo треда ему передаётся 1 байт с task_id_ задачи.
 * После передачи на выполнение, можно ждать завершения в task_wait_4end().
 *
 * Потомок инициализирует данные для задачи, из главного потока. Затем вызывает send_2thr(),
 * с указанием любого треда из пула. И задача отправится в очередь треда. С этого момента объектом
 * владеет исполняющий тред. Который вызовет task_run_function(), и после неё,
 * on_task_end() - отправку семафора.
 * Семафор задачи может ожидаться главным потоком, вызвавшим task_wait_4end(). После выхода из
 * неё, объектом снова владеет главный поток.
 */
class t_thr_task_base
{
public:
    t_thr_task_base(int task_id);
    virtual ~t_thr_task_base();

    virtual void task_run_function() = 0;
    void on_task_end();

    void task_wait_4end();

protected:
    sem_t sem_stop_;              // Чтобы ждать выполнения задания
    t_sortint *p_begin_, *p_end_; // Начало и конец обрабатываемых данных. p_end_ - элемент после последнего.
    int task_id_;                 // Уникальный индекс объекта в массиве t_vec_tasks
    bool fl_task_run_;            // Меняется из главного потока

    void send_2thr(t_thr &thr);
};

typedef std::vector<t_thr_task_base *> t_vec_tasks;

//==============================================================================
//==============================================================================

/* Класс задачи, для сортировки буфера данных
 */
class t_task_sort: public t_thr_task_base
{
public:
    t_task_sort(int task_id);
    virtual ~t_task_sort();
    void start_sort(t_thr &thr, t_sortint *p_begin, t_sortint *p_end, bool fl_asc);

    virtual void task_run_function();

private:
    bool fl_asc_;
};

//==============================================================================
//==============================================================================

/* Класс для одного треда. Имеет ссылку на вектор указателей на задачи, каждая из которых
 * может через очередь fifo прийти на исполнение. Сам вектор используется только на чтение.
 */
class t_thr
{
public:
    t_thr(t_vec_tasks &vec_tasks);
    ~t_thr();

    void thr_add_task(int task_id);
    void thr_stop();

    static const int TASK_ID_STOP = 0x80;


private:
    bool fl_thr_active_;
    int fd_fifo_write_2thr_;
    int fd_fifo_read_from_thr_;
    pthread_t thr_sys_id_;

    t_vec_tasks &vec_tasks_;

    static void *thr_sys_run_function(void *arg);
    void thr_internal_run_function();
};

typedef std::vector<t_thr *> t_vec_thr;


//==============================================================================
//==============================================================================



#endif // THR_H
