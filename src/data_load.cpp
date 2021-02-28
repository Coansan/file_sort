#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>

#include "data_load.h"


//==============================================================================
//==============================================================================

void t_file_loader::ldr_set_mem(t_sortint *p_begin, t_sortint *p_end)
{   // Установка пользовательского буфера. Не меняет состояние файла. Необходимо для работы
    p_begin_ = p_begin;
    p_end_   = p_end;
    buf_sortint_sz_ = (NULL != p_begin && NULL != p_end) ? p_end - p_begin : 0;
}

//==============================================================================

bool t_file_loader::ldr_fopen(const char *fname, bool fl_load_not_save)
{   // Открываем новый файл на чтение при (fl_load_not_save == true).
    // Иначе - на запись.
    // true - если успешно.
    ldr_fclose();
    if(NULL == fname) {
        return false;
    }

    fl_load_not_save_ = fl_load_not_save;
    int flags = (fl_load_not_save) ? O_RDONLY : O_WRONLY + O_CREAT + O_TRUNC;
    fd_ = open(fname, flags, 0666);
    return fd_ >= 0;
}

//==============================================================================

size_t t_file_loader::ldr_load_2mem()
{   /* Загружаем очередную порцию данных в буфер. Возвращаем загруженный размер в словах.
     * Если вернулось 0, то произошла ошибка, или файл кончился. В этом случае файл будет закрыт.
     * Если заполнили часть буфера, и файл кончился, вернёт размер данных,
     * а о конце файла станет известно при следующем вызове.
     */
    if(fd_ < 0 || !fl_load_not_save_ || NULL == p_begin_ || NULL == p_end_) {
        return 0;
    }

    size_t bytes_sz = buf_sortint_sz_ * sizeof(t_sortint);
    uint8_t *p_rd = reinterpret_cast<uint8_t *>(p_begin_);

    while(0 != bytes_sz) {
        const ssize_t r = read(fd_, p_rd, bytes_sz);
        if(0 >= r || r > bytes_sz) {
            // Ошибка или конец файла
            ldr_fclose();
            break;
        }
        bytes_sz -= r;
        p_rd     += r;
    }

    return (p_rd - reinterpret_cast<uint8_t *>(p_begin_)) / sizeof(t_sortint);
}

//==============================================================================

bool t_file_loader::ldr_save_2file()
{   /* Пытаемся записать весь буфер в файл. Возврат true, если нет ошибок.
     * Пользователь сам закрывает файл (ldr_fclose()), когда это нужно.
     * Файл должен быть открыт на запись.
     */
    if(fd_ < 0 || fl_load_not_save_ || NULL == p_begin_ || NULL == p_end_) {
        return false;
    }

    size_t bytes_sz = buf_sortint_sz_ * sizeof(t_sortint);
    uint8_t *p_wr = reinterpret_cast<uint8_t *>(p_begin_);
    bool res = true;

    while(0 != bytes_sz) {
        const ssize_t r = write(fd_, p_wr, bytes_sz);
        if(0 >= r || r > bytes_sz) {
            // Ошибка
            ldr_fclose();
            res = false;
            break;
        }
        bytes_sz -= r;
        p_wr     += r;
    }

    return res;
}

//==============================================================================

void t_file_loader::ldr_fclose(bool fl_sync)
{   // Закрывает файл, если он открыт.
    if(fd_ >= 0) {
        if(fl_sync) {
            fsync(fd_);
        }
        close(fd_);
        fd_ = -1;
    }
}


//==============================================================================
//==============================================================================
//==============================================================================
//=============== t_data_stream ================================================


t_data_stream::t_data_stream():
    p_begin_(NULL), p_end_(NULL), p_4get_(NULL),
    remain_size_(0), p_file_(NULL)
{
}

//==============================================================================

t_data_stream::~t_data_stream()
{
    if(NULL != p_file_) {
        p_file_->ldr_fclose();
    }
}

//==============================================================================

void t_data_stream::dstr_set_mem(t_sortint *p_begin, t_sortint *p_end, bool fl_contains_data)
{   // Установка буфера необходима для работы.
    // Если подключен файл, буфер устанавливается и для него.
    // Если fl_contains_data == true, будет считаться, что буфер содержит данные,
    // иначе - пустой.
    p_begin_ = p_begin;
    p_end_   = p_end;
    fl_contains_data = fl_contains_data && NULL != p_begin && NULL != p_end &&
                       p_end > p_begin;

    if(fl_contains_data) {
        p_4get_      = p_begin;
        remain_size_ = p_end - p_begin;
    } else {
        p_4get_      = NULL;
        remain_size_ = 0;
    }

    if(NULL != p_file_) {
        p_file_->ldr_set_mem(p_begin, p_end);
    }
}

//==============================================================================

void t_data_stream::dstr_set_file_ldr(t_file_loader *p_file)
{   // В любой момент работы, можно установить новый объект файла,
    // открытый для чтения.
    if(NULL != p_file_) {
        p_file_->ldr_fclose();
    }
    p_file_ = p_file;
    p_file->ldr_set_mem(p_begin_, p_end_);
}

//==============================================================================

bool t_data_stream::dstr_load_data()
{   // Пытается загрузить буфер из файла, предыдущее содержимое буфера теряется.
    // true, если есть загруженные данные.
    // Может вызываться как из dstr_get_word(), так и пользователем.
    if(NULL != p_file_ &&
       (remain_size_ = p_file_->ldr_load_2mem()) > 0) {

        p_4get_ = p_begin_;
        return true;
    }

    p_4get_      = NULL;
    remain_size_ = 0;
    return false;
}

