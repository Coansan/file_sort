#ifndef DATA_LOAD_H
#define DATA_LOAD_H


#include <stdint.h>
#include <string.h>

#include "thr.h"


/* Если файл открыт для чтения, обеспечивает загрузку файла в буфер, указанный пользователем.
 * Загрузка может выполняться последовательными частями, по размеру буфера.
 *
 * Также, можно открыть файл на запись, и выполнять запись буфера в файл.
 */
class t_file_loader
{
public:
    t_file_loader():
        p_begin_(NULL), p_end_(NULL), buf_sortint_sz_(0), fd_(-1), fl_load_not_save_(true) {}
    ~t_file_loader() { ldr_fclose(); }

    void ldr_set_mem(t_sortint *p_begin, t_sortint *p_end);
    bool ldr_fopen(const char *fname, bool fl_load_not_save);
    size_t ldr_load_2mem();
    bool ldr_save_2file();
    void ldr_fclose(bool fl_sync = false);
    bool ldr_file_is_open() { return fd_ >= 0; }

private:
    t_sortint *p_begin_, *p_end_; // Буфер установленный пользователем
    size_t buf_sortint_sz_;       // Полный размер буфера, в словах (t_sortint)
    int fd_;                      // Дескриптор, если < 0, то сейчас нет файла в работе
    bool fl_load_not_save_;       // true - загрузка в память, false - сохранение в файл
};

//==============================================================================
//==============================================================================

/* Класс потока данных. Умеет выдавать по 1 слову, это удобно для операции слияния.
 * Можно, но не обязательно, подключить объект t_file_loader, в котором уже открыт файл.
 * Тогда, при отсутствии данных в буфере, они подгружаются из файла.
 */
class t_data_stream
{
public:
    t_data_stream();
    ~t_data_stream();

    void dstr_set_mem(t_sortint *p_begin, t_sortint *p_end, bool fl_contains_data = false);
    void dstr_set_file_ldr(t_file_loader *p_file);
    bool dstr_load_data();
    size_t dstr_get_remain_size()  const { return remain_size_; }
    t_sortint *dstr_get_data_ptr() const { return p_4get_; }
    t_sortint *dstr_get_p_end()    const { return p_end_; }
    inline bool dstr_get_word(t_sortint *p_dst);

private:
    t_sortint *p_begin_, *p_end_; // Буфер установленный пользователем
    t_sortint *p_4get_;           // Указывает в буфере на первое слово данных, которое ещё не выведено dstr_get_word()
    size_t remain_size_;          // Сколько в буфере есть слов для вывода
    t_file_loader *p_file_;       // Объект для подгрузки из файла, может быть NULL
};

typedef std::vector<t_data_stream> t_vec_data_stream;

//==============================================================================


inline bool t_data_stream::dstr_get_word(t_sortint *p_dst)
{   /* Возвращает очередное слово данных в *p_dst. true если успешно.
     * Когда данные в буфере кончились, может загрузить из файла следующую порцию.
     */
    if(0 != remain_size_ || dstr_load_data()) {
        --remain_size_;
        *p_dst = *(p_4get_++);
        return true;
    }

    return false;
}




#endif // DATA_LOAD_H
