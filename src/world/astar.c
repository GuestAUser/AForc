/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "world_private.h"

#include <string.h>

#define PATH_CARDINAL_COST UINT64_C(10)
#define PATH_DIAGONAL_COST UINT64_C(14)
#define PATH_KNOWN_FLAGS                                                       \
    ((uint32_t)AFORC_PATH_ALLOW_DIAGONAL |                                     \
     (uint32_t)AFORC_PATH_PREVENT_CORNER_CUTTING)

typedef enum PathNodeState
{
    PATH_NODE_UNSEEN = 0,
    PATH_NODE_OPEN,
    PATH_NODE_CLOSED
} PathNodeState;

typedef struct PathNode
{
    size_t parent;
    size_t heap_position;
    uint64_t cost;
    uint64_t heuristic;
    uint64_t score;
    uint16_t epoch;
    PathNodeState state;
} PathNode;

typedef struct PathHeap
{
    size_t *indices;
    size_t count;
    size_t capacity;
    PathNode *nodes;
} PathHeap;

typedef struct PathRequest
{
    AFORC_PathOptions options;
    bool diagonal;
    bool prevent_corner_cutting;
} PathRequest;

struct AFORC_PathWorkspace
{
    AFORC_Allocator allocator;
    PathNode *nodes;
    size_t *heap_indices;
    AFORC_Point *path_points;
    size_t capacity;
    uint16_t epoch;
};

static bool path_u64_add(uint64_t left, uint64_t right, uint64_t *out_sum)
{
    if (out_sum == NULL || right > UINT64_MAX - left)
    {
        return false;
    }
    *out_sum = left + right;
    return true;
}

static bool
path_u64_multiply(uint64_t left, uint64_t right, uint64_t *out_product)
{
    if (out_product == NULL || (left != 0U && right > UINT64_MAX / left))
    {
        return false;
    }
    *out_product = left * right;
    return true;
}

static AFORC_Point path_point_from_index(const AFORC_TileMap *map, size_t index)
{
    const size_t width = (size_t)map->size.width;
    AFORC_Point point;

    point.x = (int32_t)(index % width);
    point.y = (int32_t)(index / width);
    return point;
}

AFORC_PathOptions aforc_path_options_default(void)
{
    const AFORC_PathOptions options = {AFORC_PATH_NONE, 0U};
    return options;
}

AFORC_Status aforc_path_workspace_create(const AFORC_Allocator *allocator,
                                         AFORC_PathWorkspace **out_workspace)
{
    AFORC_PathWorkspace *workspace = NULL;
    AFORC_Status status;

    if (out_workspace == NULL)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    *out_workspace = NULL;
    status = aforc_alloc_array(
        allocator, 1U, sizeof(*workspace), (void **)&workspace);
    if (status != AFORC_OK)
    {
        return status;
    }
    (void)memset(workspace, 0, sizeof(*workspace));
    workspace->allocator = *allocator;
    *out_workspace = workspace;
    return AFORC_OK;
}

AFORC_Status aforc_path_workspace_reserve(AFORC_PathWorkspace *workspace,
                                          size_t cell_capacity)
{
    PathNode *nodes = NULL;
    size_t *heap_indices = NULL;
    AFORC_Point *path_points = NULL;
    AFORC_Status status;

    if (workspace == NULL)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (cell_capacity <= workspace->capacity)
    {
        return AFORC_OK;
    }
    status = aforc_alloc_array(
        &workspace->allocator, cell_capacity, sizeof(*nodes), (void **)&nodes);
    if (status == AFORC_OK)
    {
        status = aforc_alloc_array(&workspace->allocator,
                                   cell_capacity,
                                   sizeof(*heap_indices),
                                   (void **)&heap_indices);
    }
    if (status == AFORC_OK)
    {
        status = aforc_alloc_array(&workspace->allocator,
                                   cell_capacity,
                                   sizeof(*path_points),
                                   (void **)&path_points);
    }
    if (status != AFORC_OK)
    {
        aforc_free(&workspace->allocator, path_points);
        aforc_free(&workspace->allocator, heap_indices);
        aforc_free(&workspace->allocator, nodes);
        return status;
    }
    (void)memset(nodes, 0, cell_capacity * sizeof(*nodes));
    aforc_free(&workspace->allocator, workspace->path_points);
    aforc_free(&workspace->allocator, workspace->heap_indices);
    aforc_free(&workspace->allocator, workspace->nodes);
    workspace->nodes = nodes;
    workspace->heap_indices = heap_indices;
    workspace->path_points = path_points;
    workspace->capacity = cell_capacity;
    workspace->epoch = 0U;
    return AFORC_OK;
}

void aforc_path_workspace_destroy(AFORC_PathWorkspace *workspace)
{
    if (workspace != NULL)
    {
        const AFORC_Allocator allocator = workspace->allocator;

        aforc_free(&allocator, workspace->path_points);
        aforc_free(&allocator, workspace->heap_indices);
        aforc_free(&allocator, workspace->nodes);
        aforc_free(&allocator, workspace);
    }
}

static void path_workspace_begin(AFORC_PathWorkspace *workspace)
{
    if (workspace->epoch == UINT16_MAX)
    {
        (void)memset(workspace->nodes,
                     0,
                     workspace->capacity * sizeof(*workspace->nodes));
        workspace->epoch = 1U;
    }
    else
    {
        ++workspace->epoch;
    }
}

static PathNode *path_workspace_node(AFORC_PathWorkspace *workspace,
                                     size_t node_index)
{
    PathNode *node = &workspace->nodes[node_index];

    if (node->epoch != workspace->epoch)
    {
        node->parent = SIZE_MAX;
        node->heap_position = SIZE_MAX;
        node->cost = UINT64_MAX;
        node->heuristic = UINT64_MAX;
        node->score = UINT64_MAX;
        node->state = PATH_NODE_UNSEEN;
        node->epoch = workspace->epoch;
    }
    return node;
}

static bool
path_node_less(const PathHeap *heap, size_t left_index, size_t right_index)
{
    const PathNode *left = &heap->nodes[left_index];
    const PathNode *right = &heap->nodes[right_index];

    if (left->score != right->score)
    {
        return left->score < right->score;
    }
    if (left->heuristic != right->heuristic)
    {
        return left->heuristic < right->heuristic;
    }
    return left_index < right_index;
}

static void path_heap_swap(PathHeap *heap, size_t left, size_t right)
{
    const size_t temporary = heap->indices[left];

    heap->indices[left] = heap->indices[right];
    heap->indices[right] = temporary;
    heap->nodes[heap->indices[left]].heap_position = left;
    heap->nodes[heap->indices[right]].heap_position = right;
}

static void path_heap_sift_up(PathHeap *heap, size_t position)
{
    while (position > 0U)
    {
        const size_t parent = (position - 1U) / 2U;

        if (!path_node_less(
                heap, heap->indices[position], heap->indices[parent]))
        {
            break;
        }
        path_heap_swap(heap, position, parent);
        position = parent;
    }
}

static void path_heap_sift_down(PathHeap *heap, size_t position)
{
    for (;;)
    {
        const size_t left = position * 2U + 1U;
        size_t best = position;
        size_t right;

        if (left >= heap->count)
        {
            break;
        }
        right = left + 1U;
        if (path_node_less(heap, heap->indices[left], heap->indices[best]))
        {
            best = left;
        }
        if (right < heap->count &&
            path_node_less(heap, heap->indices[right], heap->indices[best]))
        {
            best = right;
        }
        if (best == position)
        {
            break;
        }
        path_heap_swap(heap, position, best);
        position = best;
    }
}

static AFORC_Status path_heap_push(PathHeap *heap, size_t node_index)
{
    size_t position;

    if (heap->count >= heap->capacity)
    {
        return AFORC_ERROR_STATE;
    }
    position = heap->count++;
    heap->indices[position] = node_index;
    heap->nodes[node_index].heap_position = position;
    path_heap_sift_up(heap, position);
    return AFORC_OK;
}

static size_t path_heap_pop(PathHeap *heap)
{
    const size_t result = heap->indices[0];

    --heap->count;
    heap->nodes[result].heap_position = SIZE_MAX;
    if (heap->count != 0U)
    {
        heap->indices[0] = heap->indices[heap->count];
        heap->nodes[heap->indices[0]].heap_position = 0U;
        path_heap_sift_down(heap, 0U);
    }
    return result;
}

static AFORC_Status path_heuristic(const AFORC_TileMap *map,
                                   size_t from_index,
                                   size_t goal_index,
                                   bool diagonal,
                                   uint64_t *out_heuristic)
{
    const AFORC_Point from = path_point_from_index(map, from_index);
    const AFORC_Point goal = path_point_from_index(map, goal_index);
    const uint64_t delta_x = from.x >= goal.x
                                 ? (uint64_t)((int64_t)from.x - goal.x)
                                 : (uint64_t)((int64_t)goal.x - from.x);
    const uint64_t delta_y = from.y >= goal.y
                                 ? (uint64_t)((int64_t)from.y - goal.y)
                                 : (uint64_t)((int64_t)goal.y - from.y);
    uint64_t first;
    uint64_t second;

    if (out_heuristic == NULL)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (!diagonal)
    {
        if (!path_u64_add(delta_x, delta_y, &first) ||
            !path_u64_multiply(first, PATH_CARDINAL_COST, out_heuristic))
        {
            return AFORC_ERROR_OVERFLOW;
        }
        return AFORC_OK;
    }
    first = delta_x < delta_y ? delta_x : delta_y;
    second = delta_x > delta_y ? delta_x - delta_y : delta_y - delta_x;
    if (!path_u64_multiply(first, PATH_DIAGONAL_COST, &first) ||
        !path_u64_multiply(second, PATH_CARDINAL_COST, &second) ||
        !path_u64_add(first, second, out_heuristic))
    {
        return AFORC_ERROR_OVERFLOW;
    }
    return AFORC_OK;
}

static AFORC_Status path_write(const AFORC_TileMap *map,
                               const PathNode *nodes,
                               size_t start_index,
                               size_t goal_index,
                               AFORC_Point *out_points,
                               size_t point_capacity,
                               size_t *out_length)
{
    size_t cursor = goal_index;
    size_t length = 1U;
    size_t output_index;

    while (cursor != start_index)
    {
        if (nodes[cursor].parent == SIZE_MAX || length >= map->cell_count)
        {
            return AFORC_ERROR_STATE;
        }
        cursor = nodes[cursor].parent;
        ++length;
    }
    *out_length = length;
    if (out_points == NULL || point_capacity < length)
    {
        return AFORC_ERROR_LIMIT;
    }
    cursor = goal_index;
    output_index = length;
    while (output_index != 0U)
    {
        out_points[--output_index] = path_point_from_index(map, cursor);
        if (cursor == start_index)
        {
            break;
        }
        cursor = nodes[cursor].parent;
    }
    return AFORC_OK;
}

static AFORC_Status path_request_prepare(const AFORC_TileMap *map,
                                         uint32_t layer,
                                         AFORC_Point start,
                                         AFORC_Point goal,
                                         AFORC_TileTestFn is_blocked,
                                         void *context,
                                         const AFORC_PathOptions *options,
                                         PathRequest *out_request)
{
    AFORC_Status status =
        aforc_world_validate_layer_point_internal(map, layer, start);

    if (status != AFORC_OK)
    {
        return status;
    }
    status = aforc_world_validate_layer_point_internal(map, layer, goal);
    if (status != AFORC_OK)
    {
        return status;
    }
    out_request->options = aforc_path_options_default();
    if (options != NULL)
    {
        out_request->options = *options;
    }
    if ((out_request->options.flags & ~PATH_KNOWN_FLAGS) != 0U)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    out_request->diagonal =
        (out_request->options.flags & AFORC_PATH_ALLOW_DIAGONAL) != 0U;
    out_request->prevent_corner_cutting =
        (out_request->options.flags & AFORC_PATH_PREVENT_CORNER_CUTTING) != 0U;
    if (aforc_world_tile_matches_internal(
            map, layer, start, is_blocked, context))
    {
        return AFORC_ERROR_NOT_FOUND;
    }
    return AFORC_OK;
}

static AFORC_Status path_search(AFORC_PathWorkspace *workspace,
                                const AFORC_TileMap *map,
                                uint32_t layer,
                                AFORC_Point start,
                                AFORC_Point goal,
                                AFORC_TileTestFn is_blocked,
                                void *context,
                                const PathRequest *request,
                                AFORC_Point *out_points,
                                size_t point_capacity,
                                size_t *out_length)
{
    static const int32_t directions[8][2] = {
        {0, -1}, {-1, 0}, {1, 0}, {0, 1}, {-1, -1}, {1, -1}, {-1, 1}, {1, 1}};
    PathHeap heap;
    size_t start_index;
    size_t goal_index;
    size_t visit_limit;
    size_t visited = 0U;
    size_t direction_count;
    AFORC_Status status;

    path_workspace_begin(workspace);
    heap.indices = workspace->heap_indices;
    heap.count = 0U;
    heap.capacity = map->cell_count;
    heap.nodes = workspace->nodes;
    start_index = aforc_world_point_index_internal(map, start);
    goal_index = aforc_world_point_index_internal(map, goal);
    path_workspace_node(workspace, start_index)->cost = 0U;
    status = path_heuristic(map,
                            start_index,
                            goal_index,
                            request->diagonal,
                            &workspace->nodes[start_index].heuristic);
    if (status != AFORC_OK)
    {
        return status;
    }
    workspace->nodes[start_index].score =
        workspace->nodes[start_index].heuristic;
    workspace->nodes[start_index].state = PATH_NODE_OPEN;
    status = path_heap_push(&heap, start_index);
    if (status != AFORC_OK)
    {
        return status;
    }
    visit_limit = request->options.max_visited == 0U ||
                          request->options.max_visited > map->cell_count
                      ? map->cell_count
                      : request->options.max_visited;
    direction_count = request->diagonal ? 8U : 4U;

    while (heap.count != 0U)
    {
        size_t node_index;
        size_t direction_index;
        AFORC_Point current;

        if (visited >= visit_limit)
        {
            return AFORC_ERROR_LIMIT;
        }
        node_index = path_heap_pop(&heap);
        workspace->nodes[node_index].state = PATH_NODE_CLOSED;
        ++visited;
        if (node_index == goal_index)
        {
            return path_write(map,
                              workspace->nodes,
                              start_index,
                              goal_index,
                              out_points,
                              point_capacity,
                              out_length);
        }
        current = path_point_from_index(map, node_index);
        for (direction_index = 0U; direction_index < direction_count;
             ++direction_index)
        {
            const int32_t offset_x = directions[direction_index][0];
            const int32_t offset_y = directions[direction_index][1];
            const int64_t neighbor_x = (int64_t)current.x + offset_x;
            const int64_t neighbor_y = (int64_t)current.y + offset_y;
            AFORC_Point neighbor;
            size_t neighbor_index;
            PathNode *neighbor_node;
            uint64_t step_cost;
            uint64_t candidate_cost;
            uint64_t candidate_score;
            AFORC_Status operation_status;

            if (!aforc_world_coordinates_contained_internal(
                    map, neighbor_x, neighbor_y))
            {
                continue;
            }
            neighbor.x = (int32_t)neighbor_x;
            neighbor.y = (int32_t)neighbor_y;
            neighbor_index = aforc_world_point_index_internal(map, neighbor);
            neighbor_node = path_workspace_node(workspace, neighbor_index);
            if (neighbor_node->state == PATH_NODE_CLOSED)
            {
                continue;
            }
            if (offset_x != 0 && offset_y != 0 &&
                request->prevent_corner_cutting)
            {
                const AFORC_Point horizontal = {neighbor.x, current.y};
                const AFORC_Point vertical = {current.x, neighbor.y};

                if (aforc_world_tile_matches_internal(
                        map, layer, horizontal, is_blocked, context) ||
                    aforc_world_tile_matches_internal(
                        map, layer, vertical, is_blocked, context))
                {
                    continue;
                }
            }
            if (aforc_world_tile_matches_internal(
                    map, layer, neighbor, is_blocked, context))
            {
                continue;
            }
            step_cost = offset_x != 0 && offset_y != 0 ? PATH_DIAGONAL_COST
                                                       : PATH_CARDINAL_COST;
            if (!path_u64_add(workspace->nodes[node_index].cost,
                              step_cost,
                              &candidate_cost))
            {
                return AFORC_ERROR_OVERFLOW;
            }
            if (neighbor_node->state == PATH_NODE_OPEN &&
                candidate_cost >= neighbor_node->cost)
            {
                continue;
            }
            if (neighbor_node->state == PATH_NODE_UNSEEN)
            {
                operation_status = path_heuristic(map,
                                                  neighbor_index,
                                                  goal_index,
                                                  request->diagonal,
                                                  &neighbor_node->heuristic);
                if (operation_status != AFORC_OK)
                {
                    return operation_status;
                }
            }
            if (!path_u64_add(
                    candidate_cost, neighbor_node->heuristic, &candidate_score))
            {
                return AFORC_ERROR_OVERFLOW;
            }
            neighbor_node->parent = node_index;
            neighbor_node->cost = candidate_cost;
            neighbor_node->score = candidate_score;
            if (neighbor_node->state == PATH_NODE_UNSEEN)
            {
                neighbor_node->state = PATH_NODE_OPEN;
                operation_status = path_heap_push(&heap, neighbor_index);
                if (operation_status != AFORC_OK)
                {
                    return operation_status;
                }
            }
            else
            {
                path_heap_sift_up(&heap, neighbor_node->heap_position);
            }
        }
    }
    return AFORC_ERROR_NOT_FOUND;
}

AFORC_Status aforc_pathfind_astar_workspace(AFORC_PathWorkspace *workspace,
                                            const AFORC_TileMap *map,
                                            uint32_t layer,
                                            AFORC_Point start,
                                            AFORC_Point goal,
                                            AFORC_TileTestFn is_blocked,
                                            void *context,
                                            const AFORC_PathOptions *options,
                                            const AFORC_Point **out_points,
                                            size_t *out_length)
{
    PathRequest request;
    AFORC_Status status;

    if (workspace == NULL || out_points == NULL || out_length == NULL)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    *out_points = NULL;
    *out_length = 0U;
    status = path_request_prepare(
        map, layer, start, goal, is_blocked, context, options, &request);
    if (status != AFORC_OK)
    {
        return status;
    }
    if (workspace->capacity < map->cell_count)
    {
        return AFORC_ERROR_LIMIT;
    }
    if (aforc_world_point_equal(start, goal))
    {
        workspace->path_points[0] = start;
        *out_points = workspace->path_points;
        *out_length = 1U;
        return AFORC_OK;
    }
    status = path_search(workspace,
                         map,
                         layer,
                         start,
                         goal,
                         is_blocked,
                         context,
                         &request,
                         workspace->path_points,
                         workspace->capacity,
                         out_length);
    if (status == AFORC_OK)
    {
        *out_points = workspace->path_points;
    }
    return status;
}

AFORC_Status aforc_pathfind_astar(const AFORC_TileMap *map,
                                  uint32_t layer,
                                  AFORC_Point start,
                                  AFORC_Point goal,
                                  AFORC_TileTestFn is_blocked,
                                  void *context,
                                  const AFORC_PathOptions *options,
                                  AFORC_Point *out_points,
                                  size_t point_capacity,
                                  size_t *out_length)
{
    AFORC_PathWorkspace workspace;
    PathRequest request;
    AFORC_Status status;

    if (out_length == NULL || (out_points == NULL && point_capacity != 0U))
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    *out_length = 0U;
    status = path_request_prepare(
        map, layer, start, goal, is_blocked, context, options, &request);
    if (status != AFORC_OK)
    {
        return status;
    }
    if (aforc_world_point_equal(start, goal))
    {
        *out_length = 1U;
        if (out_points == NULL || point_capacity == 0U)
        {
            return AFORC_ERROR_LIMIT;
        }
        out_points[0] = start;
        return AFORC_OK;
    }

    (void)memset(&workspace, 0, sizeof(workspace));
    workspace.allocator = map->allocator;
    workspace.capacity = map->cell_count;
    status = aforc_alloc_array(&workspace.allocator,
                               workspace.capacity,
                               sizeof(*workspace.nodes),
                               (void **)&workspace.nodes);
    if (status == AFORC_OK)
    {
        status = aforc_alloc_array(&workspace.allocator,
                                   workspace.capacity,
                                   sizeof(*workspace.heap_indices),
                                   (void **)&workspace.heap_indices);
    }
    if (status != AFORC_OK)
    {
        aforc_free(&workspace.allocator, workspace.heap_indices);
        aforc_free(&workspace.allocator, workspace.nodes);
        return status;
    }
    (void)memset(
        workspace.nodes, 0, workspace.capacity * sizeof(*workspace.nodes));
    status = path_search(&workspace,
                         map,
                         layer,
                         start,
                         goal,
                         is_blocked,
                         context,
                         &request,
                         out_points,
                         point_capacity,
                         out_length);
    aforc_free(&workspace.allocator, workspace.heap_indices);
    aforc_free(&workspace.allocator, workspace.nodes);
    return status;
}
