#ifndef SORTER_H
#define SORTER_H


#include <string>

#include "thr.h"
#include "data_load.h"


//==============================================================================


class t_sorter
{
public:
    t_sorter(int threads_n, size_t mem_bytes_sz, bool fl_asc);
    ~t_sorter();
    static int srt_get_min_mem_size_mb() { return ((sort_buf_min_sortint_sz_ + 2 * fwrite_buf_sortint_sz_)
                                                   / sizeof(t_sortint)) >> 20;
                                         }

    bool srt_stage1(const char *input_fname);

private:
    std::string &get_fname(int stage_n, int file_n);
    bool merge_data(t_vec_data_stream &vec_data_stream, t_file_loader &file_dest);

    static constexpr const char *const tmp_fname_base_     = "tmp_qj9HZ7rW_";
    static constexpr const char *const out_fname_          = "sort_result.bin";
    static constexpr const size_t fwrite_buf_sortint_sz_   = 0x0800000 / sizeof(t_sortint); //  8 MB
    static constexpr const size_t sort_buf_min_sortint_sz_ = 0x1000000 / sizeof(t_sortint); // 16 MB, минимальный размер для mem_sortint_sz_
    static const int merge_files_max_k_                    = 16;

    bool fl_asc_;            // По возрастанию или убыванию сортировать

    t_vec_tasks vec_tasks_;  // Задачи, которые можем исполнять. Их может быть больше, чем тредов
    t_vec_thr vec_thr_;      // Пул тредов

    t_sortint *p_sort_mem_;    // Большой буфер, выделенный для обработки данных. Может делиться между заданиями для тредов.
    size_t mem_sortint_sz_;    // Его размер
    t_sortint *fwrite_bufs[2]; // Буфера размером fwrite_buf_sortint_sz_, для записи в файл

    // Для формирования имён файлов
    const std::string str_tmp_fname_base_;
    std::string str_tmp_fname_;

};


#endif // SORTER_H
