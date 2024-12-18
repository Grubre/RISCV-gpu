#include "Vgpu_gpu.h"
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>
#include "sim.hpp"
#include "instructions.hpp"

using namespace sim::instructions;

constexpr auto INST_NUM_CHANNELS = Vgpu_gpu::INSTRUCTION_MEM_NUM_CHANNELS;
constexpr auto DATA_NUM_CHANNELS = Vgpu_gpu::DATA_MEM_NUM_CHANNELS;

TEST_CASE("mov + sw + halt") {
    auto gpu = Vgpu{};

    auto instruction_memory = sim::make_instruction_memory<1024, INST_NUM_CHANNELS>(&gpu);
    auto data_memory = sim::make_data_memory<1024, DATA_NUM_CHANNELS>(&gpu);

    instruction_memory.push_instruction(addi(5, 1, 0));
    instruction_memory.push_instruction(sw(5, 1, 0));
    instruction_memory.push_instruction(halt());

    sim::set_kernel_config(gpu, 0, 0, 1, 1);

    auto done = sim::simulate(gpu, instruction_memory, data_memory, 100);

    REQUIRE(done);
    for(auto i = 0; i < 32; i++) {
        CHECK(data_memory[i] == i);
    }
}

TEST_CASE("lw + sw") {
    auto gpu = Vgpu{};

    auto instruction_memory = sim::make_instruction_memory<1024, INST_NUM_CHANNELS>(&gpu);
    auto data_memory = sim::make_data_memory<1024, DATA_NUM_CHANNELS>(&gpu);

    data_memory.push_data(10);
    data_memory.push_data(20);
    data_memory.push_data(30);

    instruction_memory.push_instruction(lw(6, 0, 0));
    instruction_memory.push_instruction(sw(1, 6, 0));
    instruction_memory.push_instruction(halt());

    sim::set_kernel_config(gpu, 0, 0, 1, 1);

    auto done = simulate(gpu, instruction_memory, data_memory, 10000);

    REQUIRE(done);
    for(auto i = 0; i < 32; i++) {
        CHECK(data_memory[i] == 10);
    }
}

TEST_CASE("add") {
    auto top = Vgpu{};

    auto data_mem = sim::make_data_memory<2048, 8>(&top);
    auto instruction_mem = sim::make_instruction_memory<2048, 8>(&top);

    data_mem.push_data(10);
    data_mem.push_data(20);

    instruction_mem.push_instruction(lw(6, 0, 0));
    instruction_mem.push_instruction(lw(5, 0, 1));
    instruction_mem.push_instruction(add(7, 6, 5));
    instruction_mem.push_instruction(sw(1, 7, 0));
    instruction_mem.push_instruction(halt());


    // Prepare kernel configuration
    sim::set_kernel_config(top, 0, 0, 1, 1);

    // Run simulation
    auto done = simulate(top, instruction_mem, data_mem, 2000);

    REQUIRE(done);
    for(auto i = 0; i < 32; i++) {
        CHECK(data_mem[i] == 30);
    }
}

TEST_CASE("mask") {
    constexpr auto mem_cells_count = 2048;

    auto top = Vgpu{};

    auto data_mem = sim::make_data_memory<mem_cells_count, DATA_NUM_CHANNELS>(&top);
    auto instruction_mem = sim::make_instruction_memory<mem_cells_count, INST_NUM_CHANNELS>(&top);

    data_mem.push_data(IData{1} << 2);

    auto mask_instruction = lw(1, 0, 0);
    mask_instruction.bits |= 1 << 6;

    instruction_mem.push_instruction(mask_instruction);
    instruction_mem.push_instruction(addi(5, 1 ,0));
    instruction_mem.push_instruction(sw(5, 1, 0));
    instruction_mem.push_instruction(halt());

    sim::set_kernel_config(top, 0, 0, 1, 1);

    auto done = simulate(top, instruction_mem, data_mem, 500);

    REQUIRE(done);

    CHECK(data_mem[0] == 4);
    for(auto i = 1; i < 32; i++) {
        if(i == 2) {
            CHECK(data_mem[i] == 2);
        } else {
            CHECK(data_mem[i] == 0);
        }
    }
}

TEST_CASE("sx_slti") {
    auto top = Vgpu{};

    auto data_mem = sim::make_data_memory<2048, 8>(&top);
    auto instruction_mem = sim::make_instruction_memory<2048, 8>(&top);

    instruction_mem.push_instruction(addi(5, 1 ,0));
    instruction_mem.push_instruction(sx_slti(1, 5, 5));
    instruction_mem.push_instruction(sw(5, 1, 0));
    instruction_mem.push_instruction(halt());

    // Prepare kernel configuration
    sim::set_kernel_config(top, 0, 0, 1, 1);

    // Run simulation
    auto done = simulate(top, instruction_mem, data_mem, 2000);

    REQUIRE(done);
    for(auto i = 0; i < 32; i++) {
        if(i < 5) {
            CHECK(data_mem[i] == i);
        } else {
            CHECK(data_mem[i] == 0);
        }
    }
}