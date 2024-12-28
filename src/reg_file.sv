module reg_file #(
    parameter int THREADS_PER_WARP = 32,
    parameter int DATA_WIDTH = `DATA_WIDTH
) (
    input wire clk,
    input wire reset,
    input wire enable, // Warp enable signal

    // Per-thread enable signals (execution mask)
    input logic [THREADS_PER_WARP-1:0] thread_enable,

    // Warp and block identifiers
    input data_t warp_id,
    input data_t block_id,
    input data_t block_size,

    // Decoded instruction fields
    input logic decoded_reg_write_enable,      // Write enable for the register file
    input logic [1:0] decoded_reg_input_mux,   // Determines the source of data to write
    input data_t decoded_immediate,            // Immediate value for constant writes
    input logic [4:0] decoded_rd_address,      // Destination register index
    input logic [4:0] decoded_rs1_address,     // Source register 1 index
    input logic [4:0] decoded_rs2_address,     // Source register 2 index

    // Inputs from ALU and LSU per thread
    input data_t alu_out      [THREADS_PER_WARP],
    input data_t lsu_out      [THREADS_PER_WARP],

    // Outputs per thread
    output data_t rs1         [THREADS_PER_WARP],
    output data_t rs2         [THREADS_PER_WARP]
);

// Special-purpose register indices
localparam int ZERO_REG = 0;
localparam int THREAD_ID_REG = 1;
localparam int BLOCK_ID_REG = 2;
localparam int BLOCK_SIZE_REG = 3;

// Register file: each thread has its own set of 32 registers
data_t registers [THREADS_PER_WARP][32];

// Compute thread IDs per thread during reset
data_t thread_ids [THREADS_PER_WARP];
always_comb begin
    for (int i = 0; i < THREADS_PER_WARP; i++) begin
        thread_ids[i] = warp_id * THREADS_PER_WARP + i;
    end
end

// Initialize registers during reset and handle writes
always @(posedge clk) begin
    if (reset) begin
        for (int i = 0; i < THREADS_PER_WARP; i++) begin
            registers[i][ZERO_REG]     <= {DATA_WIDTH{1'b0}};
            registers[i][THREAD_ID_REG]<= thread_ids[i];
            registers[i][BLOCK_ID_REG] <= block_id;
            registers[i][BLOCK_SIZE_REG]<= block_size;
            for (int j = 4; j < 32; j++) begin
                registers[i][j] <= {DATA_WIDTH{1'b0}};
            end
        end
    end else if (enable && decoded_reg_write_enable) begin
        for (int i = 0; i < THREADS_PER_WARP; i++) begin
            if (thread_enable[i]) begin
                // Prevent writes to read-only registers
                if (decoded_rd_address >= 4) begin
                    case (decoded_reg_input_mux)
                        2'b00: registers[i][decoded_rd_address] <= alu_out[i];          // ALU result
                        2'b01: registers[i][decoded_rd_address] <= lsu_out[i];          // LSU result
                        2'b10: registers[i][decoded_rd_address] <= decoded_immediate;   // Immediate value
                        default: $error("Invalid decoded_reg_input_mux value");
                    endcase
                end else begin
                    $error("Attempted to write to read-only register %d", decoded_rd_address);
                end
            end
        end
    end
end

// Read register values
always_comb begin
    for (int i = 0; i < THREADS_PER_WARP; i++) begin
        if (thread_enable[i]) begin
            rs1[i] = registers[i][decoded_rs1_address];
            rs2[i] = registers[i][decoded_rs2_address];
        end else begin
            rs1[i] = {DATA_WIDTH{1'b0}};
            rs2[i] = {DATA_WIDTH{1'b0}};
        end
    end
end

endmodule
