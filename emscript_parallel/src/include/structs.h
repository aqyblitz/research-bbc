#ifndef __STRUCTS_H__
#define __STRUCTS_H__

// System Include
#include <stdint.h>
#include <vector>
#include <cereal/types/vector.hpp>
#include <cereal/archives/portable_binary.hpp>

///////////////////////////////////////////////////////////////////////////////
// CmdVector
///////////////////////////////////////////////////////////////////////////////
typedef struct
{
    // Variables
    int32_t 			   c;
    // Usage:
    // - 0 for requesting a row for broadcast
    // - 1 for sending a row (broadcasting it for computation)
    // - 2 for retrieving rows for the solution
    // - 3 for shutdown
    int32_t                      k; // relevant k value
    std::vector<int32_t>        data; // relevant command data

    // Serialize Method
    template <class Archive>
    void serialize(Archive & archive)
    {
        archive(c, k, data);
    }

} CmdVector;

///////////////////////////////////////////////////////////////////////////////
// DataVector
///////////////////////////////////////////////////////////////////////////////
typedef struct
{
    // Variables
    std::vector<int32_t>         data; // row data from request

    // Serialize Method
    template <class Archive>
    void serialize(Archive & archive)
    {
        archive(data);
    }
} DataVector;

///////////////////////////////////////////////////////////////////////////////
// InitVector
///////////////////////////////////////////////////////////////////////////////
typedef struct
{
    // Variables
    int32_t 			   v_t; // vertex_total
    int32_t                      id; // node ID. node_id/(vertex_total/block_total) = node_id/block_size = block_id
    int32_t                      b_t; // block total
    std::vector<int32_t>         data; // block of adj_matrix

    // Serialize Method
    template <class Archive>
    void serialize(Archive & archive)
    {
        archive(v_t, id, b_t, data);
    }
} InitVector;

///////////////////////////////////////////////////////////////////////////////
// AckMsg
///////////////////////////////////////////////////////////////////////////////
typedef struct
{
    // Variables
    int32_t 			   status; // code to indicate success
    int32_t                      id; // block ID

    // Serialize Method
    template <class Archive>
    void serialize(Archive & archive)
    {
        archive(status, id);
    }
} AckMsg;

#endif 
