#pragma once
#include <print>
#include <array>
#include "Vgpu.h"
#include "instructions.hpp"

namespace sim {

constexpr void tick(Vgpu& top) {
    top.clk = 0;
    top.eval();
    top.clk = 1;
    top.eval();
}

// CData is uint8_t, IData is uint32_t
constexpr void set_bit(CData& signal, int bit, bool value) {
    if (value) {
        signal |= 1 << bit;
    } else {
        signal &= ~(1 << bit);
    }
}

constexpr bool get_bit(CData signal, int bit) {
    return (signal >> bit) & 1;
}

template <uint32_t mem_cells, uint32_t num_channels>
struct InstructionMemory {
    Vgpu* dut;
    CData *instruction_mem_read_valid;                  // input
    IData *instruction_mem_read_address[num_channels];  // input
    CData *instruction_mem_read_ready;                  // output
    IData *instruction_mem_read_data[num_channels];     // output

    std::array<IData, mem_cells> memory{};

    // Process read requests
    void process() {
        for (size_t i = 0; i < num_channels; i++) {
            if (*instruction_mem_read_valid & (1 << i)) {
                IData addr = *instruction_mem_read_address[i];
                if (addr < mem_cells) {
                    *instruction_mem_read_data[i] = memory[addr];
                } else {
                    *instruction_mem_read_data[i] = 0;
                    std::println(stderr, "Error: Read from invalid address {}", addr);
                }
                set_bit(*instruction_mem_read_ready, (int)i, true);
            } else {
                set_bit(*instruction_mem_read_ready, (int)i, false);
            }
        }
    }

    // Method to load an instruction into memory
    void load_instruction(IData addr, IData instruction) {
        if (addr < mem_cells) {
            memory[addr] = instruction;
        } else {
            std::println(stderr, "Error: Attempt to load instruction at invalid address {}", addr);
        }
    }

    void push_instruction(InstructionBits instruction) {
        memory[stack_ptr++] = (IData)instruction;
    }

    auto operator[](IData addr) -> IData& {
        return memory[addr];
    }

    uint32_t stack_ptr = 0u;
};

template <uint32_t mem_cells, uint32_t num_channels>
struct DataMemory {
    Vgpu* dut;
    CData *data_mem_read_valid;                  // input
    IData *data_mem_read_address[num_channels];  // input
    CData *data_mem_read_ready;                  // output
    IData *data_mem_read_data[num_channels];     // output
    CData *data_mem_write_valid;                 // input
    IData *data_mem_write_address[num_channels]; // input
    IData *data_mem_write_data[num_channels];    // input
    CData *data_mem_write_ready;                 // output

    std::array<IData, mem_cells> memory{};

    auto operator[](IData addr) -> IData& {
        return memory[addr];
    }

    // Process read and write requests
    void process() {
        // Process writes first
        for (size_t i = 0; i < num_channels; i++) {
            if (*data_mem_write_valid & (1 << i)) {
                IData addr = *data_mem_write_address[i];
                if (addr < mem_cells) {
                    memory[addr] = *data_mem_write_data[i];
                } else {
                    std::println(stderr, "Error: Write to invalid address {}", addr);
                }
                set_bit(*data_mem_write_ready, (int)i, true);
            } else {
                set_bit(*data_mem_write_ready, (int)i, false);
            }
        }

        // Then process reads
        for (size_t i = 0; i < num_channels; i++) {
            if (*data_mem_read_valid & (1 << i)) {
                IData addr = *data_mem_read_address[i];
                if (addr < mem_cells) {
                    *data_mem_read_data[i] = memory[addr];
                } else {
                    data_mem_read_data[i] = 0;
                    std::println(stderr, "Error: Read from invalid address {}", addr);
                }
                set_bit(*data_mem_read_ready, (int)i, true);
            } else {
                set_bit(*data_mem_read_ready, (int)i, false);
            }
        }
    }

    // Optional: Method to print memory content for debugging
    void print_memory(IData start_addr = 0, IData end_addr = mem_cells - 1) {
        for (IData addr = start_addr; addr <= end_addr && addr < mem_cells; ++addr) {
            std::println("Memory[{}]: {}", addr, memory[addr]);
        }
    }

    void push_data(IData data) {
        memory[stack_ptr++] = data;
    }

    uint32_t stack_ptr = 0u;
};

template <uint32_t mem_cells, uint32_t num_channels>
auto make_instruction_memory(Vgpu* dut) -> InstructionMemory<mem_cells, num_channels> {
    InstructionMemory<mem_cells, num_channels> mem{};
    mem.dut = dut;
    mem.instruction_mem_read_valid = &dut->instruction_mem_read_valid;
    mem.instruction_mem_read_ready = &dut->instruction_mem_read_ready;
    for (auto i = 0u; i < num_channels; i++) {
        mem.instruction_mem_read_address[i] = &dut->instruction_mem_read_address[i];
        mem.instruction_mem_read_data[i] = &dut->instruction_mem_read_data[i];
    }
    return mem;
}

template <uint32_t mem_cells, uint32_t num_channels>
auto make_data_memory(Vgpu* dut) -> DataMemory<mem_cells, num_channels> {
    DataMemory<mem_cells, num_channels> mem{};
    mem.dut = dut;
    mem.data_mem_read_valid = &dut->data_mem_read_valid;
    mem.data_mem_read_ready = &dut->data_mem_read_ready;
    mem.data_mem_write_valid = &dut->data_mem_write_valid;
    mem.data_mem_write_ready = &dut->data_mem_write_ready;
    for (auto i = 0u; i < num_channels; i++) {
        mem.data_mem_read_address[i] = &dut->data_mem_read_address[i];
        mem.data_mem_read_data[i] = &dut->data_mem_read_data[i];
        mem.data_mem_write_address[i] = &dut->data_mem_write_address[i];
        mem.data_mem_write_data[i] = &dut->data_mem_write_data[i];
    }
    return mem;
}

constexpr void set_kernel_config(Vgpu& top, IData base_instructions_address, IData base_data_address, IData num_blocks, IData num_warps_per_block) {
    VlWide<4>& kernel_config = top.kernel_config;
    kernel_config[3] = base_instructions_address;
    kernel_config[2] = base_data_address;
    kernel_config[1] = num_blocks;
    kernel_config[0] = num_warps_per_block;
}

template <uint32_t mem_cells, uint32_t num_channels>
bool simulate(Vgpu& top, InstructionMemory<mem_cells, num_channels>& instruction_mem, DataMemory<mem_cells, num_channels>& data_mem, uint32_t max_num_cycles) {
    top.execution_start = 1;

    for (auto cycle = 0u; cycle < max_num_cycles; ++cycle) {
        top.eval();

        if (top.execution_done) {
            return true;
        }

        instruction_mem.process();
        data_mem.process();

        top.eval();

        tick(top);
    }
    return false;
}

} // namespace sim

