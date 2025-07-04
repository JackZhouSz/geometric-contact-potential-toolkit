#pragma once

#include "potential.hpp"

#include <ipc/utils/local_to_global.hpp>
#include <ipc/utils/MaybeParallelFor.hpp>

#include <tbb/blocked_range.h>
#include <tbb/combinable.h>
#include <tbb/enumerable_thread_specific.h>

namespace ipc {

template <class TCollisions>
double Potential<TCollisions>::operator()(
    const TCollisions& collisions,
    const CollisionMesh& mesh,
    Eigen::ConstRef<Eigen::MatrixXd> X) const
{
    assert(X.rows() == mesh.num_vertices());

    if (collisions.empty()) {
        return 0;
    }

    tbb::enumerable_thread_specific<double> storage(0);

    tbb::parallel_for(
        tbb::blocked_range<size_t>(size_t(0), collisions.size()),
        [&](const tbb::blocked_range<size_t>& r) {
            auto& local_potential = storage.local();
            for (size_t i = r.begin(); i < r.end(); i++) {
                // Quadrature weight is premultiplied by local potential
                local_potential += (*this)(
                    collisions[i],
                    collisions[i].dof(X, mesh.edges(), mesh.faces()));
            }
        });

    return storage.combine([](double a, double b) { return a + b; });
}

template <class TCollisions>
Eigen::VectorXd Potential<TCollisions>::gradient(
    const TCollisions& collisions,
    const CollisionMesh& mesh,
    Eigen::ConstRef<Eigen::MatrixXd> X) const
{
    assert(X.rows() == mesh.num_vertices());

    if (collisions.empty()) {
        return Eigen::VectorXd::Zero(X.size());
    }

    const int dim = X.cols();

    auto storage = ipc::utils::create_thread_storage<Eigen::VectorXd>(
        Eigen::VectorXd::Zero(X.size()));
    ipc::utils::maybe_parallel_for(
        collisions.size(), [&](int start, int end, int thread_id) {
            auto& global_grad =
                ipc::utils::get_local_thread_storage(storage, thread_id);

            for (size_t i = start; i < end; i++) {
                const TCollision& collision = collisions[i];

                const Vector<double, -1, Potential<TCollisions>::element_size>
                    local_grad = this->gradient(
                        collision,
                        collision.dof(X, mesh.edges(), mesh.faces()));

                const std::array<index_t, TCollision::element_size> vids =
                    collision.vertex_ids(mesh.edges(), mesh.faces());

                local_gradient_to_global_gradient(
                    local_grad, vids, dim, global_grad);
            }
        });

    Eigen::VectorXd grad;
    grad.setZero(X.size());
    for (const auto& local_storage : storage)
        grad += local_storage;
    return grad;
}

template <class TCollisions>
Eigen::SparseMatrix<double> Potential<TCollisions>::hessian(
    const TCollisions& collisions,
    const CollisionMesh& mesh,
    Eigen::ConstRef<Eigen::MatrixXd> X,
    const PSDProjectionMethod project_hessian_to_psd) const
{
    assert(X.rows() == mesh.num_vertices());

    if (collisions.empty()) {
        return Eigen::SparseMatrix<double>(X.size(), X.size());
    }

    const Eigen::MatrixXi& edges = mesh.edges();
    const Eigen::MatrixXi& faces = mesh.faces();

    const int dim = X.cols();
    const int ndof = X.size();

    const int max_triplets_size = int(1e7);
    const int buffer_size = std::min(max_triplets_size, ndof);
    auto storage = ipc::utils::create_thread_storage(
        LocalThreadMatStorage(buffer_size, ndof, ndof));
    ipc::utils::maybe_parallel_for(
        collisions.size(), [&](int start, int end, int thread_id) {
            auto& hess_triplets =
                ipc::utils::get_local_thread_storage(storage, thread_id);

            for (size_t i = start; i < end; i++) {
                const TCollision& collision = collisions[i];

                const MatrixMax<
                    double, Potential<TCollisions>::element_size,
                    Potential<TCollisions>::element_size>
                    local_hess = this->hessian(
                        collisions[i], collisions[i].dof(X, edges, faces),
                        project_hessian_to_psd);

                const std::array<index_t, TCollision::element_size> vids =
                    collision.vertex_ids(edges, faces);

                local_hessian_to_global_triplets(
                    local_hess, vids, dim, *(hess_triplets.cache));
            }
        });

    Eigen::SparseMatrix<double> hess(ndof, ndof);

    // Assemble the stiffness matrix by concatenating the tuples in each local
    // storage

    // Collect thread storages
    std::vector<LocalThreadMatStorage*> storages(storage.size());
    int index = 0;
    for (auto& local_storage : storage) {
        storages[index++] = &local_storage;
    }

    utils::maybe_parallel_for(
        storages.size(), [&](int i) { storages[i]->cache->prune(); });

    if (storage.size() == 0) {
        return Eigen::SparseMatrix<double>();
    }

    // Prepares for parallel concatenation
    std::vector<int> offsets(storage.size());

    index = 0;
    int triplet_count = 0;
    for (auto& local_storage : storage) {
        offsets[index++] = triplet_count;
        triplet_count += local_storage.cache->triplet_count();
    }

    std::vector<Eigen::Triplet<double>> triplets;

    assert(storages.size() >= 1);
    if (storages[0]->cache->is_dense()) {
        // Serially merge local storages
        Eigen::MatrixXd tmp(hess);
        for (const auto& local_storage : storage)
            tmp += dynamic_cast<const DenseMatrixCache&>(*local_storage.cache)
                       .mat();
        hess = tmp.sparseView();
        hess.makeCompressed();
    } else if (triplet_count >= triplets.max_size()) {
        // Serial fallback version in case the vector of triplets cannot be
        // allocated

        logger().warn(
            "Cannot allocate space for triplets, switching to serial assembly.");

        // Serially merge local storages
        for (LocalThreadMatStorage& local_storage : storage)
            hess += local_storage.cache->get_matrix(false); // will also prune
        hess.makeCompressed();
    } else {
        triplets.resize(triplet_count);

        // Parallel copy into triplets
        utils::maybe_parallel_for(storages.size(), [&](int i) {
            const SparseMatrixCache& cache =
                dynamic_cast<const SparseMatrixCache&>(*storages[i]->cache);
            int offset = offsets[i];

            std::copy(
                cache.entries().begin(), cache.entries().end(),
                triplets.begin() + offset);
            offset += cache.entries().size();

            if (cache.mat().nonZeros() > 0) {
                int count = 0;
                for (int k = 0; k < cache.mat().outerSize(); ++k) {
                    for (Eigen::SparseMatrix<double>::InnerIterator it(
                             cache.mat(), k);
                         it; ++it) {
                        assert(count < cache.mat().nonZeros());
                        triplets[offset + count++] = Eigen::Triplet<double>(
                            it.row(), it.col(), it.value());
                    }
                }
            }
        });

        // Sort and assemble
        hess.setFromTriplets(triplets.begin(), triplets.end());
    }

    return hess;
}

} // namespace ipc