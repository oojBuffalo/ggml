#include "ggml.h"

#include "mnist-common.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <thread>
#include <vector>

#if defined(_MSC_VER)
#pragma warning(disable: 4244 4267) // possible loss of data
#endif

int main(int argc, char ** argv) {
    srand(time(NULL));
    ggml_time_init();

    if (argc != 4 && argc != 5) {
        fprintf(stderr, "Usage: %s mnist-fc-f32.gguf data/MNIST/raw/t10k-images-idx3-ubyte data/MNIST/raw/t10k-labels-idx1-ubyte [CPU/CUDA0]\n", argv[0]);
        exit(1);
    }

    std::vector<float> images;
    images.resize(MNIST_NTEST*MNIST_NINPUT);
    if (!mnist_image_load(argv[2], images.data(), MNIST_NTEST)) {
        return 1;
    }

    std::vector<float> labels;
    labels.resize(MNIST_NTEST*MNIST_NCLASSES);
    if (!mnist_label_load(argv[3], labels.data(), MNIST_NTEST)) {
        return 1;
    }

    const int iex = rand() % MNIST_NTEST;
    const std::vector<float> digit(images.begin() + iex*MNIST_NINPUT, images.begin() + (iex+1)*MNIST_NINPUT);

    mnist_image_print(stdout, images.data() + iex*MNIST_NINPUT);

    const std::string backend = argc >= 5 ? argv[4] : "CPU";

    mnist_eval_result result_eval;

    if (backend == "CPU") {
        const int ncores_logical = std::thread::hardware_concurrency();
        result_eval = mnist_graph_eval(
            argv[1], images.data(), labels.data(), MNIST_NTEST, std::min(ncores_logical, (ncores_logical + 4)/2));
        if (result_eval.success) {
            fprintf(stdout, "%s: predicted digit is %d\n", __func__, result_eval.pred[iex]);

            std::pair<double, double> result_loss = mnist_loss(result_eval);
            fprintf(stdout, "%s: test_loss=%.6lf+-%.6lf\n", __func__, result_loss.first, result_loss.second);

            std::pair<double, double> result_acc = mnist_accuracy(result_eval);
            fprintf(stdout, "%s: test_acc=%.2lf+-%.2lf%%\n", __func__, 100.0*result_acc.first, 100.0*result_acc.second);

            return 0;
        }
    } else {
        fprintf(stdout, "%s: not trying to load a GGML graph from %s because this is only supported for the CPU backend\n", __func__, argv[1]);
    }

    const int64_t t_start_us = ggml_time_us();

    mnist_model model = mnist_model_init_from_file(argv[1], backend);

    mnist_model_build(model, MNIST_NBATCH_LOGICAL, MNIST_NBATCH_PHYSICAL);

    const int64_t t_load_us = ggml_time_us() - t_start_us;

    fprintf(stdout, "%s: loaded model in %.2lf ms\n", __func__, t_load_us / 1000.0);
    result_eval = mnist_model_eval(model, images.data(), labels.data(), MNIST_NTEST);
    fprintf(stdout, "%s: predicted digit is %d\n", __func__, result_eval.pred[iex]);

    std::pair<double, double> result_loss = mnist_loss(result_eval);
    fprintf(stdout, "%s: test_loss=%.6lf+-%.6lf\n", __func__, result_loss.first, result_loss.second);

    std::pair<double, double> result_acc = mnist_accuracy(result_eval);
    fprintf(stdout, "%s: test_acc=%.2lf+-%.2lf%%\n", __func__, 100.0*result_acc.first, 100.0*result_acc.second);

    return 0;
}
