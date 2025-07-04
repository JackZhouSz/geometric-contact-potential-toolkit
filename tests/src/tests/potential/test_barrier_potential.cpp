#include <tests/config.hpp>
#include <tests/utils.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>

#include <ipc/potentials/barrier_potential.hpp>
#include <ipc/distance/edge_edge_mollifier.hpp>
#include <ipc/distance/point_point.hpp>
#include <ipc/distance/point_edge.hpp>
#include <ipc/utils/local_to_global.hpp>
#include <ipc/distance/line_line.hpp>

#include <finitediff.hpp>
#include <igl/edges.h>
#include <ipc/ipc.hpp>

using namespace ipc;

TEST_CASE(
    "Barrier potential full gradient and hessian",
    "[potential][barrier_potential][gradient][hessian]")
{
    const auto broad_phase = GENERATE(tests::BroadPhaseGenerator::create());
#ifndef NDEBUG
    if (broad_phase->name() == "BruteForce") {
        SKIP("Brute force is too slow in debug mode");
    }
#endif

    const bool use_area_weighting = GENERATE(true, false);
    const bool use_improved_max_approximator = GENERATE(true, false);
    const bool use_physical_barrier = GENERATE(true, false);

    double dhat = -1;
    std::string mesh_name = "";
    bool all_vertices_on_surface = true;
    SECTION("cube")
    {
        dhat = sqrt(2.0);
        mesh_name = "cube.ply";
    }
#ifdef NDEBUG
    SECTION("two cubes far")
    {
        dhat = 1e-1;
        mesh_name = "two-cubes-far.ply";
        all_vertices_on_surface = false;
    }
    SECTION("two cubes close")
    {
        dhat = 1e-1;
        mesh_name = "two-cubes-close.ply";
        all_vertices_on_surface = false;
    }
#endif
    // WARNING: The bunny takes too long in debug.
    // SECTION("bunny")
    // {
    //     dhat = 1e-2;
    //     mesh_name = "bunny.ply";
    // }

    Eigen::MatrixXd vertices;
    Eigen::MatrixXi edges, faces;
    bool success = tests::load_mesh(mesh_name, vertices, edges, faces);
    CAPTURE(mesh_name);
    REQUIRE(success);

    CollisionMesh mesh;

    NormalCollisions collisions;
    collisions.set_use_area_weighting(use_area_weighting);
    collisions.set_use_improved_max_approximator(use_improved_max_approximator);
    if (all_vertices_on_surface) {
        mesh = CollisionMesh(vertices, edges, faces);
    } else {
        mesh = CollisionMesh::build_from_full_mesh(vertices, edges, faces);
        vertices = mesh.vertices(vertices);
    }
    collisions.build(mesh, vertices, dhat, /*dmin=*/0, broad_phase);
    CAPTURE(
        dhat, broad_phase->name(), all_vertices_on_surface, use_area_weighting,
        use_improved_max_approximator);
    CHECK(collisions.size() > 0);

    BarrierPotential barrier_potential(dhat, use_physical_barrier);

    // -------------------------------------------------------------------------
    // Gradient
    // -------------------------------------------------------------------------

    const Eigen::VectorXd grad_b =
        barrier_potential.gradient(collisions, mesh, vertices);

    // Compute the gradient using finite differences
    Eigen::VectorXd fgrad_b;
    {
        auto f = [&](const Eigen::VectorXd& x) {
            return barrier_potential(
                collisions, mesh, fd::unflatten(x, vertices.cols()));
        };
        fd::finite_gradient(fd::flatten(vertices), f, fgrad_b);
    }

    REQUIRE(grad_b.squaredNorm() > 0);
    CHECK(fd::compare_gradient(grad_b, fgrad_b));

    // -------------------------------------------------------------------------
    // Hessian
    // -------------------------------------------------------------------------

    Eigen::MatrixXd hess_b =
        barrier_potential.hessian(collisions, mesh, vertices);

    // Compute the gradient using finite differences
    Eigen::MatrixXd fhess_b;
    {
        auto f = [&](const Eigen::VectorXd& x) {
            return barrier_potential.gradient(
                collisions, mesh, fd::unflatten(x, vertices.cols()));
        };
        fd::finite_jacobian(fd::flatten(vertices), f, fhess_b);
    }

    REQUIRE(hess_b.squaredNorm() > 0);
    CHECK(fd::compare_hessian(hess_b, fhess_b, 1e-3));
}

TEST_CASE(
    "Barrier potential convergent formulation",
    "[potential][barrier_potential][convergent]")
{
    const bool use_area_weighting = GENERATE(true, false);
    const bool use_improved_max_approximator = GENERATE(true, false);
    const bool use_physical_barrier = GENERATE(true, false);
    const double dhat = 1e-3;

    Eigen::MatrixXd vertices;
    Eigen::MatrixXi edges, faces;
    SECTION("2D Edge-Vertex")
    {
        //        .
        // .-------.-------.
        vertices.resize(4, 2);
        vertices.row(0) << 0, 1e-4;
        vertices.row(1) << -1, 0;
        vertices.row(2) << 1e-4, 0;
        vertices.row(3) << 1, 0;

        edges.resize(2, 2);
        edges.row(0) << 1, 2;
        edges.row(1) << 2, 3;

        CHECK(
            point_point_distance(vertices.row(0), vertices.row(2))
            < dhat * dhat);
    }
    SECTION("3D Face-Vertex")
    {
        vertices.resize(5, 3);
        vertices.row(0) << 0, 1e-4, 0;
        vertices.row(1) << -1, 0, 0;
        vertices.row(2) << 1e-4, 0, -1;
        vertices.row(3) << 1e-4, 0, 1;
        vertices.row(4) << 1, 0, 0;

        faces.resize(2, 3);
        faces.row(0) << 1, 2, 3;
        faces.row(1) << 2, 3, 4;

        igl::edges(faces, edges);

        CHECK(
            point_edge_distance(
                vertices.row(0), vertices.row(2), vertices.row(3))
            < dhat * dhat);
    }
    SECTION("3D Edge-Edge")
    {
        vertices.resize(5, 3);
        //
        vertices.row(0) << 0, 1e-4, -1;
        vertices.row(1) << 0, 1e-4, 1;
        //
        vertices.row(2) << -1e-4, 0, 0;
        vertices.row(3) << -1, 0, 0;
        vertices.row(4) << 1, 0, 0;

        edges.resize(3, 2);
        edges.row(0) << 0, 1;
        edges.row(1) << 3, 2;
        edges.row(2) << 2, 4;

        CHECK(
            point_edge_distance(
                vertices.row(2), vertices.row(0), vertices.row(1))
            < dhat * dhat);
    }
    SECTION("3D Edge-Edge Parallel")
    {
        vertices.resize(5, 3);
        //
        vertices.row(0) << -0.5, 1e-5, -1e-3;
        vertices.row(1) << 0.5, 1e-5, 1e-3;
        //
        vertices.row(2) << -1, -1e-5, 0;
        vertices.row(3) << 0, -1e-5, 0;
        vertices.row(4) << 1, -1e-5, 0;

        edges.resize(3, 2);
        edges.row(0) << 0, 1;
        edges.row(1) << 2, 3;
        edges.row(2) << 3, 4;

        CHECK(
            point_edge_distance(
                vertices.row(3), vertices.row(0), vertices.row(1))
            < dhat * dhat);
    }

    const CollisionMesh mesh(vertices, edges, faces);

    NormalCollisions collisions;
    collisions.set_use_area_weighting(use_area_weighting);
    collisions.set_use_improved_max_approximator(use_improved_max_approximator);

    collisions.build(mesh, vertices, dhat);
    CHECK(collisions.size() > 0);

    BarrierPotential barrier_potential(dhat, use_physical_barrier);

    const Eigen::VectorXd grad_b =
        barrier_potential.gradient(collisions, mesh, vertices);

    // Compute the gradient using finite differences
    auto f = [&](const Eigen::VectorXd& x) {
        const Eigen::MatrixXd fd_V = fd::unflatten(x, mesh.dim());

        NormalCollisions fd_collisions;
        fd_collisions.set_use_area_weighting(use_area_weighting);
        fd_collisions.set_use_improved_max_approximator(
            use_improved_max_approximator);

        fd_collisions.build(mesh, fd_V, dhat);

        return barrier_potential(collisions, mesh, fd_V);
    };
    Eigen::VectorXd fgrad_b;
    fd::finite_gradient(fd::flatten(vertices), f, fgrad_b);

    CHECK(fd::compare_gradient(grad_b, fgrad_b));
}

TEST_CASE(
    "Barrier potential shape derivative",
    "[potential][barrier_potential][shape_derivative]")
{
    Eigen::MatrixXd vertices;
    Eigen::MatrixXi edges, faces;
    tests::load_mesh("cube.ply", vertices, edges, faces);

    const bool use_area_weighting = GENERATE(false);
    const bool use_improved_max_approximator = GENERATE(true, false);
    const bool use_physical_barrier = GENERATE(true, false);
    const double dhat = 1e-1;

    // Stack cube on top of itself
    edges.conservativeResize(edges.rows() * 2, edges.cols());
    edges.bottomRows(edges.rows() / 2) =
        edges.topRows(edges.rows() / 2).array() + vertices.rows();

    faces.conservativeResize(faces.rows() * 2, faces.cols());
    faces.bottomRows(faces.rows() / 2) =
        faces.topRows(faces.rows() / 2).array() + vertices.rows();

    vertices.conservativeResize(vertices.rows() * 2, vertices.cols());
    vertices.bottomRows(vertices.rows() / 2) =
        vertices.topRows(vertices.rows() / 2);
    vertices.bottomRows(vertices.rows() / 2).col(1).array() += 1 + 0.1 * dhat;

    // Rest positions
    Eigen::MatrixXd rest_positions = vertices;
    rest_positions.bottomRows(vertices.rows() / 2).col(1).array() += 1.0;

    // Displacements
    const Eigen::MatrixXd displacements = vertices - rest_positions;

    const int ndof = vertices.size();

    // ------------------------------------------------------------------------

    CollisionMesh mesh(rest_positions, edges, faces);
    mesh.init_area_jacobians();

    Candidates candidates;
    candidates.build(mesh, vertices, dhat);

    NormalCollisions collisions;
    collisions.set_use_area_weighting(use_area_weighting);
    collisions.set_use_improved_max_approximator(use_improved_max_approximator);
    collisions.set_enable_shape_derivatives(true);
    collisions.build(candidates, mesh, vertices, dhat);
    REQUIRE(collisions.ee_collisions.size() > 0);

    BarrierPotential barrier_potential(dhat, use_physical_barrier);

    for (int i = 0; i < collisions.size(); i++) {
        std::vector<Eigen::Triplet<double>> triplets;
        barrier_potential.shape_derivative(
            collisions[i], collisions[i].vertex_ids(edges, faces),
            collisions[i].dof(rest_positions, edges, faces),
            collisions[i].dof(vertices, edges, faces), triplets);
        Eigen::SparseMatrix<double> JF_wrt_X_sparse(ndof, ndof);
        JF_wrt_X_sparse.setFromTriplets(triplets.begin(), triplets.end());
        const Eigen::MatrixXd JF_wrt_X = Eigen::MatrixXd(JF_wrt_X_sparse);

        auto F_X = [&](const Eigen::VectorXd& x) -> Eigen::VectorXd {
            // TODO: Recompute weight based on x
            assert(use_area_weighting == false);
            // Recompute eps_x based on x
            double prev_eps_x = -1;
            if (collisions.is_edge_edge(i)) {
                EdgeEdgeNormalCollision& c =
                    dynamic_cast<EdgeEdgeNormalCollision&>(collisions[i]);
                prev_eps_x = c.eps_x;
                c.eps_x = edge_edge_mollifier_threshold(
                    x.segment<3>(3 * edges(c.edge0_id, 0)),
                    x.segment<3>(3 * edges(c.edge0_id, 1)),
                    x.segment<3>(3 * edges(c.edge1_id, 0)),
                    x.segment<3>(3 * edges(c.edge1_id, 1)));
            }

            const Eigen::MatrixXd positions =
                fd::unflatten(x, rest_positions.cols()) + displacements;
            const VectorMax12d dof = collisions[i].dof(positions, edges, faces);

            Eigen::VectorXd grad = Eigen::VectorXd::Zero(ndof);
            local_gradient_to_global_gradient(
                barrier_potential.gradient(collisions[i], dof),
                collisions[i].vertex_ids(edges, faces), vertices.cols(), grad);

            // Restore eps_x
            if (collisions.is_edge_edge(i)) {
                REQUIRE(prev_eps_x >= 0);
                dynamic_cast<EdgeEdgeNormalCollision&>(collisions[i]).eps_x =
                    prev_eps_x;
            }

            return grad;
        };

        Eigen::MatrixXd fd_JF_wrt_X;
        fd::finite_jacobian(fd::flatten(rest_positions), F_X, fd_JF_wrt_X);

        CHECKED_ELSE(fd::compare_jacobian(JF_wrt_X, fd_JF_wrt_X))
        {
            tests::print_compare_nonzero(JF_wrt_X, fd_JF_wrt_X);
        }
    }

    // ------------------------------------------------------------------------

    const Eigen::MatrixXd JF_wrt_X =
        barrier_potential.shape_derivative(collisions, mesh, vertices);

    Eigen::MatrixXd sum = Eigen::MatrixXd::Zero(ndof, ndof);
    for (int i = 0; i < collisions.size(); i++) {
        std::vector<Eigen::Triplet<double>> triplets;
        barrier_potential.shape_derivative(
            collisions[i], collisions[i].vertex_ids(edges, faces),
            collisions[i].dof(rest_positions, edges, faces),
            collisions[i].dof(vertices, edges, faces), triplets);
        Eigen::SparseMatrix<double> JF_wrt_X_sparse(ndof, ndof);
        JF_wrt_X_sparse.setFromTriplets(triplets.begin(), triplets.end());
        sum += Eigen::MatrixXd(JF_wrt_X_sparse);
    }
    CHECK(fd::compare_jacobian(JF_wrt_X, sum));

    auto F_X = [&](const Eigen::VectorXd& x) {
        const Eigen::MatrixXd fd_X = fd::unflatten(x, rest_positions.cols());
        const Eigen::MatrixXd fd_V = fd_X + displacements;

        CollisionMesh fd_mesh(fd_X, mesh.edges(), mesh.faces());

        // WARNING: This breaks the tests because EE distances are C0 when edges
        // are parallel
        // NormalCollisions fd_collisions;
        // fd_collisions.set_use_area_weighting(use_area_weighting);
        // fd_collisions.set_use_improved_max_approximator(
        //     use_improved_max_approximator);
        // fd_collisions.build(fd_mesh, fd_V, dhat);

        return barrier_potential.gradient(collisions, fd_mesh, fd_V);
    };
    Eigen::MatrixXd fd_JF_wrt_X;
    fd::finite_jacobian(fd::flatten(rest_positions), F_X, fd_JF_wrt_X);

    CHECKED_ELSE(fd::compare_jacobian(JF_wrt_X, fd_JF_wrt_X))
    {
        tests::print_compare_nonzero(JF_wrt_X, fd_JF_wrt_X);
    }
}

TEST_CASE(
    "Barrier potential shape derivative (sim data)",
    "[potential][barrier_potential][shape_derivative]")
{
    nlohmann::json data;
    {
        std::ifstream input(tests::DATA_DIR / "shape_derivative_data.json");
        REQUIRE(input.good());

        data = nlohmann::json::parse(input, nullptr, false);
        REQUIRE(!data.is_discarded());
    }

    // Parameters
    double dhat = data["dhat"];

    // Mesh
    const Eigen::MatrixXi edges = data["boundary_edges"];
    Eigen::MatrixXd X = data["boundary_nodes_pos"];
    Eigen::MatrixXd vertices = data["displaced"];

    CollisionMesh mesh = CollisionMesh::build_from_full_mesh(X, edges);
    mesh.init_area_jacobians();
    REQUIRE(mesh.are_area_jacobians_initialized());

    X = mesh.vertices(X);
    vertices = mesh.vertices(vertices);
    const Eigen::MatrixXd U = vertices - X;

    NormalCollisions collisions;
    const bool use_area_weighting = GENERATE(true, false);
    const bool use_improved_max_approximator = GENERATE(true, false);
    const bool use_physical_barrier = GENERATE(true, false);
    collisions.set_use_area_weighting(use_area_weighting);
    collisions.set_use_improved_max_approximator(use_improved_max_approximator);
    collisions.set_enable_shape_derivatives(true);
    collisions.build(mesh, vertices, dhat);

    BarrierPotential barrier_potential(dhat, use_physical_barrier);

    const Eigen::MatrixXd JF_wrt_X =
        barrier_potential.shape_derivative(collisions, mesh, vertices);

    auto F_X = [&](const Eigen::VectorXd& x) {
        const Eigen::MatrixXd fd_X = fd::unflatten(x, X.cols());
        const Eigen::MatrixXd fd_V = fd_X + U;

        CollisionMesh fd_mesh(fd_X, mesh.edges(), mesh.faces());

        NormalCollisions fd_collisions;
        fd_collisions.set_use_area_weighting(use_area_weighting);
        fd_collisions.set_use_improved_max_approximator(
            use_improved_max_approximator);
        fd_collisions.build(fd_mesh, fd_V, dhat);

        return barrier_potential.gradient(fd_collisions, fd_mesh, fd_V);
    };
    Eigen::MatrixXd fd_JF_wrt_X;
    fd::finite_jacobian(fd::flatten(X), F_X, fd_JF_wrt_X);

    CHECKED_ELSE(fd::compare_jacobian(JF_wrt_X, fd_JF_wrt_X))
    {
        tests::print_compare_nonzero(JF_wrt_X, fd_JF_wrt_X);
    }
}

// -- Benchmarking ------------------------------------------------------------

TEST_CASE(
    "Benchmark barrier potential", "[!benchmark][potential][barrier_potential]")
{
    const bool use_area_weighting = GENERATE(true, false);
    const bool use_improved_max_approximator = GENERATE(true, false);
    const bool use_physical_barrier = GENERATE(true, false);

    double dhat = -1;
    std::string mesh_name;
    SECTION("cube")
    {
        dhat = sqrt(2.0);
        mesh_name = "cube.ply";
    }
    SECTION("bunny")
    {
        dhat = 1e-2;
        mesh_name = "bunny.ply";
    }

    Eigen::MatrixXd vertices;
    Eigen::MatrixXi edges, faces;
    bool success = tests::load_mesh(mesh_name, vertices, edges, faces);
    REQUIRE(success);

    const CollisionMesh mesh(vertices, edges, faces);

    NormalCollisions collisions;
    collisions.set_use_area_weighting(use_area_weighting);
    collisions.set_use_improved_max_approximator(use_improved_max_approximator);
    collisions.build(mesh, vertices, dhat);
    CAPTURE(mesh_name, dhat);
    CHECK(collisions.size() > 0);

    BarrierPotential barrier_potential(dhat, use_physical_barrier);

    BENCHMARK("Compute barrier potential")
    {
        return barrier_potential(collisions, mesh, vertices);
    };
    BENCHMARK("Compute barrier potential gradient")
    {
        return barrier_potential.gradient(collisions, mesh, vertices);
    };
    BENCHMARK("Compute barrier potential hessian")
    {
        return barrier_potential.hessian(collisions, mesh, vertices);
    };
    BENCHMARK("Compute barrier potential hessian with PSD projection")
    {
        return barrier_potential.hessian(
            collisions, mesh, vertices, PSDProjectionMethod::CLAMP);
    };
    BENCHMARK("Compute compute_minimum_distance")
    {
        return collisions.compute_minimum_distance(mesh, vertices);
    };
}

TEST_CASE(
    "Benchmark barrier potential shape derivative",
    "[!benchmark][potential][barrier_potential][shape_derivative]")
{
    nlohmann::json data;
    {
        std::ifstream input(tests::DATA_DIR / "shape_derivative_data.json");
        REQUIRE(input.good());

        data = nlohmann::json::parse(input, nullptr, false);
        REQUIRE(!data.is_discarded());
    }

    // Parameters
    double dhat = data["dhat"];

    // Mesh
    const Eigen::MatrixXi edges = data["boundary_edges"];
    Eigen::MatrixXd X = data["boundary_nodes_pos"];
    Eigen::MatrixXd vertices = data["displaced"];

    CollisionMesh mesh = CollisionMesh::build_from_full_mesh(X, edges);
    mesh.init_area_jacobians();
    REQUIRE(mesh.are_area_jacobians_initialized());

    X = mesh.vertices(X);
    vertices = mesh.vertices(vertices);
    const Eigen::MatrixXd U = vertices - X;

    NormalCollisions collisions;
    const bool use_area_weighting = GENERATE(true, false);
    const bool use_improved_max_approximator = GENERATE(true, false);
    const bool use_physical_barrier = GENERATE(true, false);
    collisions.set_use_area_weighting(use_area_weighting);
    collisions.set_use_improved_max_approximator(use_improved_max_approximator);
    collisions.set_enable_shape_derivatives(true);
    collisions.build(mesh, vertices, dhat);

    BarrierPotential barrier_potential(dhat, use_physical_barrier);

    Eigen::SparseMatrix<double> JF_wrt_X;

    BENCHMARK("Shape Derivative")
    {
        JF_wrt_X =
            barrier_potential.shape_derivative(collisions, mesh, vertices);
    };
}
