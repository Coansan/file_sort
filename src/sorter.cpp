#include <assert.h>

#include "sorter.h"


//==============================================================================
//==============================================================================


t_sorter::t_sorter(int threads_n, size_t mem_bytes_sz, bool fl_asc):
    fl_asc_(fl_asc), str_tmp_fname_base_(tmp_fname_base_)
{   /* Выделяем буфера памяти, создаём массив задач,
     * запускаем пул тредов. После их запуска, менять vec_tasks_ нельзя.
     */

    const size_t tmp_mem_sortint_sz = mem_bytes_sz / sizeof(t_sortint);
    assert(0 < threads_n && threads_n <=32 &&
           tmp_mem_sortint_sz >= sort_buf_min_sortint_sz_ + 2 * fwrite_buf_sortint_sz_);

    mem_sortint_sz_ = tmp_mem_sortint_sz - 2 * fwrite_buf_sortint_sz_;
    p_sort_mem_     = new t_sortint[mem_sortint_sz_];
    fwrite_bufs[0]  = new t_sortint[fwrite_buf_sortint_sz_];
    fwrite_bufs[1]  = new t_sortint[fwrite_buf_sortint_sz_];

    vec_tasks_.reserve(threads_n);
    for(int i = 0; i < threads_n; ++i) {
        vec_tasks_.push_back(new t_task_sort(i));
    }

    vec_thr_.reserve(threads_n);
    for(int i = 0; i < threads_n; ++i) {
        vec_thr_.push_back(new t_thr(vec_tasks_));
    }
}

//==============================================================================

t_sorter::~t_sorter()
{   // Останавливаем и удаляем все треды, затем уже задачи
    for(auto ii = vec_thr_.begin(); vec_thr_.end() != ii; ++ii) {
        delete *ii;
    }

    for(auto ii = vec_tasks_.begin(); vec_tasks_.end() != ii; ++ii) {
        delete *ii;
    }

    delete [] p_sort_mem_;
    delete [] fwrite_bufs[0];
    delete [] fwrite_bufs[1];
}

//==============================================================================

bool t_sorter::srt_stage1(const char *input_fname)
{   // Данные исходного файла будут разделены на несколько отсортированных файлов,
    // каждый размером не более mem_sortint_sz_.
    const int thr_n = vec_thr_.size();
    t_file_loader file_src, file_dest;

    if(!file_src.ldr_fopen(input_fname, true)) {
        perror("Input fopen error!");
        return false;
    }
    file_src.ldr_set_mem(p_sort_mem_, p_sort_mem_ + mem_sortint_sz_);

    t_vec_data_stream v_mpart; // К этим потокам данных не привязываются файлы, нет проблем в размещении их в векторе
    v_mpart.reserve(thr_n);

    int file_i = 0; // Индекс текущего временного файла
    size_t data_sortint_sz;

    while(0 != (data_sortint_sz = file_src.ldr_load_2mem())) {
        // Загружены данные входного файла в доступную память. Делим загруженные данные
        // между тредами и сортируем.
        v_mpart.clear(); // TODO лишняя работа
        if(data_sortint_sz > sort_buf_min_sortint_sz_) {
            // Делим память, размер куска округляем в большую сторону
            size_t mempart_sz = (data_sortint_sz + thr_n - 1) / thr_n;
            t_sortint *p_mempart = p_sort_mem_;
            t_sortint *const p_data_end = p_sort_mem_ + data_sortint_sz;

            for(int i = 0; i < thr_n; ++i) {
                t_sortint *p_mempart_end = p_mempart + mempart_sz;
                v_mpart.emplace_back();
                v_mpart.back().dstr_set_mem(p_mempart,
                                            (p_mempart_end < p_data_end) ? p_mempart_end : p_data_end,
                                            true);
                p_mempart = p_mempart_end;
            }

        } else { // ! (data_sortint_sz > sort_buf_min_sortint_sz_)
            // Только один небольшой кусок
            v_mpart.emplace_back();
            v_mpart.back().dstr_set_mem(p_sort_mem_, p_sort_mem_ + data_sortint_sz, true);
        }

        // Сейчас у нас есть загруженные данные, разделенные на 1 или thr_n кусков.
        // Запускаем задания для тредов.
        int part_i = 0;
        for(auto ii = v_mpart.begin(); v_mpart.end() != ii; ++part_i, ++ii) {
            static_cast<t_task_sort *>(vec_tasks_.at(part_i))->start_sort(*(vec_thr_.at(part_i)),
                                                                          ii->dstr_get_data_ptr(),
                                                                          ii->dstr_get_p_end(),
                                                                          fl_asc_);
        }

        printf("t_sorter::srt_stage1(): [%2d]  Threads (%d) start\n", file_i, (int)v_mpart.size());
        fflush(stdout);

        // Дожидаемся готовности результатов тредов
        for(int i = 0; i < v_mpart.size(); ++i) {
            vec_tasks_.at(i)->task_wait_4end();
        }

        printf("t_sorter::srt_stage1(): [%2d]  Megre start\n", file_i);
        fflush(stdout);

        // Теперь отсортированные части нужно слить и записать в выходной файл
        if(!file_dest.ldr_fopen(get_fname(1, file_i).c_str(), false)) {
            perror("Output for merge fopen error 1 !");
            return false;
        }
        if(!merge_data(v_mpart, file_dest)) {
            perror("merge_data() error !");
            return false;
        }
        file_dest.ldr_fclose(true);
        printf("t_sorter::srt_stage1(): [%2d]  Megre ok\n\n", file_i);
        fflush(stdout);

        ++file_i;
    } // while(...)

    return true;
}

//==============================================================================

std::string &t_sorter::get_fname(int stage_n, int file_n)
{   // Формируем имя временного файла
    char buf[64];
    const int r = snprintf(buf, sizeof(buf), "%02d_%03d.bin", stage_n, file_n);
    assert(r < sizeof(buf) - 1);
    str_tmp_fname_  = str_tmp_fname_base_;
    str_tmp_fname_ += buf;
    return str_tmp_fname_;
}

//==============================================================================

bool t_sorter::merge_data(t_vec_data_stream &vec_data_stream, t_file_loader &file_dest)
{   /* Сливает отсортированные потоки данных в отсортированный файл.
     * Открытием и закрытием файла file_dest занимается пользователь.
     * Буфер для file_dest указываем мы.
     */
    if(!file_dest.ldr_file_is_open() || vec_data_stream.empty()) {
        return false;
    }

    // Создаём массив потоков данных, готовых давать данные. Этот массив будет перестраиваться,
    // по мере опустошения потоков данных.
    struct t_dw
    {
        t_data_stream *p_ds;
        t_sortint data_word;
    };
    t_dw dw[vec_data_stream.size()];
    int actual_dw_sz = 0;

    for(auto ii = vec_data_stream.begin(); vec_data_stream.end() != ii; ++ii) {
        t_sortint t;
        if(ii->dstr_get_word(&t)) {
            dw[actual_dw_sz  ].p_ds      = &(*ii);
            dw[actual_dw_sz++].data_word = t;
        }
    }
    if(0 == actual_dw_sz) {
        return false;
    }

    t_sortint *const data_out_buf = fwrite_bufs[0]; // Размер == fwrite_buf_sortint_sz_;
    file_dest.ldr_set_mem(data_out_buf, data_out_buf + fwrite_buf_sortint_sz_);
    size_t data_out_i = 0;
    const bool fl_asc = fl_asc_;

    while(actual_dw_sz > 1) {
        // Сливаем потоки данных. Проходим по массиву рабочих потоков данных, ищем нужный элемент,
        // выталкиваем его на выход. Пока не останется 1 поток данных.
        int ix       = 0;
        t_sortint dx = dw[ix].data_word;
        if(fl_asc) {
            // Сортировка по возрастанию, ищем минимальный элемент
            for(int i = 1; i < actual_dw_sz; ++i) {
                if(dw[i].data_word < dx) {
                    dx = dw[i].data_word;
                    ix = i;
                }
            }
        } else {
            // По убыванию
            for(int i = 1; i < actual_dw_sz; ++i) {
                if(dw[i].data_word > dx) {
                    dx = dw[i].data_word;
                    ix = i;
                }
            }
        }
        //==============================

        // Замещаем число, которое идёт на вывод, новым из потока
        if(!dw[ix].p_ds->dstr_get_word(&(dw[ix].data_word))) {
            // Поток более не работает, исключаем его
            if(actual_dw_sz - 1 != ix) {
                dw[ix] = dw[actual_dw_sz - 1];
            }
            --actual_dw_sz;
        }

        // Отправляем число на выход
        data_out_buf[data_out_i++] = dx;
        if(fwrite_buf_sortint_sz_ == data_out_i) {
            // Параметры для полного буфера установлены заранее
            data_out_i = 0;
            if(!file_dest.ldr_save_2file()) {
                return false;
            }
        }

    } // while(actual_dw_sz > 1)
    //==============================

    // Сейчас у нас есть ровно один поток, данные из которого надо отправить на выход

    // Отправим одно буферизованное число, и сбросим выходной буфер
    data_out_buf[data_out_i++] = dw[0].data_word;
    file_dest.ldr_set_mem(data_out_buf, data_out_buf + data_out_i);
    data_out_i = 0;
    if(!file_dest.ldr_save_2file()) {
        return false;
    }

    // И выводим остаток потока, через его буфер, чтобы не делать копирование
    do {
        const size_t wr_sz      = dw[0].p_ds->dstr_get_remain_size();
        if(wr_sz > 0) {
            t_sortint *const wr_ptr = dw[0].p_ds->dstr_get_data_ptr();
            file_dest.ldr_set_mem(wr_ptr, wr_ptr + wr_sz);
            if(!file_dest.ldr_save_2file()) {
                return false;
            }
        }
    } while(dw[0].p_ds->dstr_load_data());

    return true;
}

//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================


int main(int argc, const char *argv[])
{
    if(argc < 2) {
        printf("\nArguments: ./sorter <fname> [-desc] [<mem_MB> [<sort_threads_n>]]\n\n");
        return 0;
    }

    bool fl_asc = true;
    int mem_mb  = 512;
    int thr_n   = 2;

    int arg_i   = 2;
    int tmp;
    bool fl_stop_args = false;

    if(arg_i < argc && !strcmp(argv[arg_i], "-desc")) {
        fl_asc = false;
        ++arg_i;
    }

    if(arg_i < argc && 1 == sscanf(argv[arg_i], "%d", &tmp) &&
       tmp >= t_sorter::srt_get_min_mem_size_mb()) {
        mem_mb = tmp;
        ++arg_i;
    } else {
        fl_stop_args = true;
    }

    if(!fl_stop_args && arg_i < argc && 1 == sscanf(argv[arg_i], "%d", &tmp) &&
       0 < tmp && tmp <= 32) {
        thr_n = tmp;
        ++arg_i;
    } else {
        fl_stop_args = true;
    }

    //===============================================
    t_sorter sorter(thr_n, (size_t)mem_mb << 20, fl_asc);

    printf("\nt_sorter( thr_n: %2d, %4d MB, fl_asc: %d ) Created\n\n", thr_n, mem_mb, (int)fl_asc);
    fflush(stdout);

    bool res = sorter.srt_stage1(argv[1]);
    printf("srt_stage1 return: %d\n\n", (int)res);

    return 0;
}








