#pragma once

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "advanced_platformer/math/coordinates.hpp"
#include "advanced_platformer/navigation/route.hpp"
#include "advanced_platformer/navigation/traversal.hpp"
#include "support/require_same_input_program.hpp"

namespace tests
{
    // The first generated connection using the traversal, or a failure if the test map
    // did not produce one.
    inline const advanced_platformer::RouteConnection& connectionWith(
        const std::vector<advanced_platformer::RouteConnection>& connections,
        advanced_platformer::Traversal traversal)
    {
        const auto connection = std::find_if(
            connections.begin(),
            connections.end(),
            [traversal](const advanced_platformer::RouteConnection& candidate)
            { return candidate.step.traversal == traversal; });
        if (connection == connections.end())
        {
            throw std::logic_error("The expected route connection was not generated");
        }
        return *connection;
    }

    // The generated connection to the cell by the traversal, or a failure if the test map
    // did not produce one.
    inline const advanced_platformer::RouteConnection& connectionWith(
        const std::vector<advanced_platformer::RouteConnection>& connections,
        advanced_platformer::Cell destination,
        advanced_platformer::Traversal traversal)
    {
        const auto connection = std::find_if(
            connections.begin(),
            connections.end(),
            [destination, traversal](const advanced_platformer::RouteConnection& candidate)
            {
                return candidate.step.destination.cell == destination &&
                       candidate.step.traversal == traversal;
            });
        if (connection == connections.end())
        {
            throw std::logic_error("The expected route connection was not generated");
        }
        return *connection;
    }

    // The first generated jump landing above the row, or a failure if the test map did
    // not produce one.
    inline const advanced_platformer::RouteConnection& jumpUpFrom(
        const std::vector<advanced_platformer::RouteConnection>& connections,
        int row)
    {
        const auto jump = std::find_if(
            connections.begin(),
            connections.end(),
            [row](const advanced_platformer::RouteConnection& connection)
            {
                return connection.step.traversal == advanced_platformer::Traversal::Jump &&
                       connection.step.destination.cell.y < row;
            });
        if (jump == connections.end())
        {
            throw std::logic_error("No jump lands above the row");
        }
        return *jump;
    }

    inline void requireSameRouteConnections(
        const std::vector<advanced_platformer::RouteConnection>& left,
        const std::vector<advanced_platformer::RouteConnection>& right)
    {
        REQUIRE(left.size() == right.size());
        for (std::size_t index = 0; index < left.size(); ++index)
        {
            REQUIRE(left[index].sourceSurface == right[index].sourceSurface);
            REQUIRE(left[index].step.destination.cell == right[index].step.destination.cell);
            REQUIRE(left[index].step.destination.surface == right[index].step.destination.surface);
            REQUIRE(left[index].step.traversal == right[index].step.traversal);
            REQUIRE(left[index].cost == right[index].cost);
            requireSameInputProgram(left[index].step.inputs, right[index].step.inputs);
        }
    }
}
