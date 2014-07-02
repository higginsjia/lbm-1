#pragma once

namespace lbm
{

///////////////////////////////////// NonFluidCollision /////////////////////////////////////////

template<typename lattice_model>
NonFluidCollision<lattice_model>::NonFluidCollision(Domain<lattice_model>& domain) :
        domain ( domain )
{
}

template<typename lattice_model>
bool NonFluidCollision<lattice_model>::is_fluid() const
{
    return false;
}

///////////////////////////////////// NoSlipBoundary /////////////////////////////////////////

template<typename lattice_model>
NoSlipBoundary<lattice_model>::NoSlipBoundary(Domain<lattice_model>& domain) :
        NonFluidCollision<lattice_model>(domain)
{
}

template<typename lattice_model>
void NoSlipBoundary<lattice_model>::collide(Cell<lattice_model>& cell,
        const uint_array<lattice_model::D>& position) const
{
    auto x = position[0];
    auto y = position[1];
    auto z = position[2];

    for (auto q = 0u; q < lattice_model::Q; ++q) {
        auto dx = lattice_model::velocities[q][0];
        auto dy = lattice_model::velocities[q][1];
        auto dz = lattice_model::velocities[q][2];
        if (this->domain.in_bounds(x + dx, y + dy, z + dz)
                && this->domain.cell(x + dx, y + dy, z + dz).is_fluid()) {
            cell[q] = this->domain.cell(x + dx, y + dy, z + dz)[lattice_model::inv(q)];
        }
    }
}

///////////////////////////////////// MovingWallBoundary /////////////////////////////////////////

template<typename lattice_model>
MovingWallBoundary<lattice_model>::MovingWallBoundary(
        const double_array<lattice_model::D>& wall_velocity, Domain<lattice_model>& domain) :
        NonFluidCollision<lattice_model>(domain), wall_velocity(wall_velocity)
{
}

template<typename lattice_model>
void MovingWallBoundary<lattice_model>::collide(Cell<lattice_model>& cell,
        const uint_array<lattice_model::D>& position) const
{
    auto x = position[0];
    auto y = position[1];
    auto z = position[2];

    for (auto q = 0u; q < lattice_model::Q; ++q) {
        auto dx = lattice_model::velocities[q][0];
        auto dy = lattice_model::velocities[q][1];
        auto dz = lattice_model::velocities[q][2];
        if (this->domain.in_bounds(x + dx, y + dy, z + dz)
                && this->domain.cell(x + dx, y + dy, z + dz).is_fluid()) {
            const auto& neighbor = this->domain.cell(x + dx, y + dy, z + dz);

            double density = this->compute_density(neighbor);
            double c_dot_u = 0;
            for (auto d = 0u; d < lattice_model::D; ++d) {
                c_dot_u += lattice_model::velocities[q][d] * wall_velocity[d];
            }
            double finv = neighbor[lattice_model::inv(q)];
            cell[q] = finv + 2.0 * lattice_model::weights[q] * density * c_dot_u / (C_S * C_S);
        }
    }
}

///////////////////////////////////// FreeSlipBoundary /////////////////////////////////////////

template<typename lattice_model>
FreeSlipBoundary<lattice_model>::FreeSlipBoundary(Domain<lattice_model>& domain) :
        NonFluidCollision<lattice_model>(domain)
{

}

template<typename lattice_model>
void FreeSlipBoundary<lattice_model>::collide(Cell<lattice_model>& cell,
        const uint_array<lattice_model::D>& position) const
{
    auto x = position[0];
    auto y = position[1];
    auto z = position[2];

    for (auto q = 0u; q < lattice_model::Q; ++q) {
        auto dx = lattice_model::velocities[q][0];
        auto dy = lattice_model::velocities[q][1];
        auto dz = lattice_model::velocities[q][2];
        if (this->domain.in_bounds(x + dx, y + dy, z + dz)
                && this->domain.cell(x + dx, y + dy, z + dz).is_fluid()) {
            if (this->domain.cell(x + dx, y, z).is_fluid())
                cell[q] =
                        this->domain.cell(x + dx, y, z)[lattice_model::velocity_index(-dx, dy, dz)];
            else if (this->domain.cell(x, y + dx, z).is_fluid())
                cell[q] =
                        this->domain.cell(x, y + dx, z)[lattice_model::velocity_index(dx, -dy, dz)];
            else if (this->domain.cell(x, y, z + dz).is_fluid())
                cell[q] =
                        this->domain.cell(x, y, z + dz)[lattice_model::velocity_index(dx, dy, -dz)];
            else // TODO: Check if this condition is correct!
                cell[q] = this->domain.cell(x + dx, y + dy, z + dz)[lattice_model::inv(q)];
        }
    }
}

///////////////////////////////////// OutflowBoundary /////////////////////////////////////////

template<typename lattice_model>
OutflowBoundary<lattice_model>::OutflowBoundary(Domain<lattice_model>& domain,
        double reference_density) :
        NonFluidCollision<lattice_model>(domain),
        reference_density { reference_density }
{

}

template<typename lattice_model>
void OutflowBoundary<lattice_model>::collide(Cell<lattice_model>& cell,
        const uint_array<lattice_model::D>& position) const
{
    auto x = position[0];
    auto y = position[1];
    auto z = position[2];

    for (auto q = 0u; q < lattice_model::Q; ++q) {
        auto dx = lattice_model::velocities[q][0];
        auto dy = lattice_model::velocities[q][1];
        auto dz = lattice_model::velocities[q][2];
        if (this->domain.in_bounds(x + dx, y + dy, z + dz)
                && this->domain.cell(x + dx, y + dy, z + dz).is_fluid()) {

            const auto& neighbor = this->domain.cell(x + dx, y + dy, z + dz);
            auto velocity = neighbor.velocity(reference_density);
            auto feq = neighbor.equilibrium(reference_density, velocity);

            cell[q] = feq[q] + feq[lattice_model::inv(q)] - neighbor[lattice_model::inv(q)];
        }
    }
}

///////////////////////////////////// InflowBoundary /////////////////////////////////////////

template<typename lattice_model>
InflowBoundary<lattice_model>::InflowBoundary(Domain<lattice_model>& domain,
        const double_array<lattice_model::D>& inflow_velocity, double reference_density) :
        NonFluidCollision<lattice_model>(domain),
        reference_density { reference_density },
        inflow_velocity(inflow_velocity)
{

}

template<typename lattice_model>
void InflowBoundary<lattice_model>::collide(Cell<lattice_model>& cell,
        const uint_array<lattice_model::D>& position) const
{
    auto x = position[0];
    auto y = position[1];
    auto z = position[2];

    for (auto q = 0u; q < lattice_model::Q; ++q) {
        auto dx = lattice_model::velocities[q][0];
        auto dy = lattice_model::velocities[q][1];
        auto dz = lattice_model::velocities[q][2];
        if (this->domain.in_bounds(x + dx, y + dy, z + dz)
                && this->domain.cell(x + dx, y + dy, z + dz).is_fluid()) {
            cell[q] = this->compute_feq(reference_density, inflow_velocity)[q];
        }
    }
}

///////////////////////////////////// PressureBoundary /////////////////////////////////////////

template<typename lattice_model>
PressureBoundary<lattice_model>::PressureBoundary(Domain<lattice_model>& domain,
        double input_density) :
        NonFluidCollision<lattice_model>(domain),
        input_density { input_density }
{

}

template<typename lattice_model>
void PressureBoundary<lattice_model>::collide(Cell<lattice_model>& cell,
        const uint_array<lattice_model::D>& position) const
{
    auto x = position[0];
    auto y = position[1];
    auto z = position[2];

    for (auto q = 0u; q < lattice_model::Q; ++q) {
        auto dx = lattice_model::velocities[q][0];
        auto dy = lattice_model::velocities[q][1];
        auto dz = lattice_model::velocities[q][2];
        if (this->domain.in_bounds(x + dx, y + dy, z + dz)
                && this->domain.cell(x + dx, y + dy, z + dz).is_fluid()) {
            auto neighbor = this->domain.cell(x + dx, y + dy, z + dz);
            auto velocity = neighbor.velocity(input_density);
            auto feq = neighbor.equilibrium(input_density, velocity);
            cell[q] = feq[q] + feq[lattice_model::inv(q)] - neighbor[lattice_model::inv(q)];
        }
    }
}
}//namespace lbm