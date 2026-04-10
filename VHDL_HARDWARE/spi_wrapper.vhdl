library IEEE;
use IEEE.STD_LOGIC_1164.ALL;

entity spi_wrapper is
Port ( 
    clk_100mhz: in std_logic; 
    sclk: in std_logic; 
    cs: in std_logic; 
    mosi: in std_logic;
    load_x: in std_logic;
    miso: out std_logic;
    reset: in std_logic 
);
end spi_wrapper;

architecture Behavioral of spi_wrapper is

component fir_filter is
generic (
    w1: integer := 16; 
    w2: integer := 32; 
    w3: integer := 36; 
    w4: integer := 16; 
    L : integer := 64  
);
Port ( 
    clk: in std_logic;
    reset: in std_logic;
    load_x: in std_logic;
    enable: in std_logic; 
    x_in : in std_logic_vector(w1-1 downto 0); 
    c_in : in std_logic_vector(w1-1 downto 0);
    y_out: out std_logic_vector(w4-1 downto 0)
);
end component;

signal shift_reg_rx   : std_logic_vector(15 downto 0) := (others => '0');
signal shift_reg_tx   : std_logic_vector(15 downto 0) := (others => '0');
signal fir_y_out      : std_logic_vector(15 downto 0);
signal cs_reg1        : std_logic := '1';
signal cs_reg2        : std_logic := '1';
signal fir_enable     : std_logic := '0';
signal sclk_reg1      : std_logic := '1';
signal sclk_reg2      : std_logic := '1';
signal bit_count      : integer range 0 to 15 := 0;

begin

filter_instance: fir_filter 
port map(
    clk    => clk_100mhz,
    reset  => reset,
    load_x => load_x,
    enable => fir_enable,
    x_in   => shift_reg_rx,
    c_in   => shift_reg_rx,
    y_out  => fir_y_out
);

miso <= shift_reg_tx(15) when cs = '0' else 'Z';

main_process: process(clk_100mhz, reset)
begin
    if reset = '1' then
        sclk_reg1    <= '1';
        sclk_reg2    <= '1';
        cs_reg1      <= '1';
        cs_reg2      <= '1';
        shift_reg_rx <= (others => '0');
        shift_reg_tx <= (others => '0');
        fir_enable   <= '0';
        bit_count    <= 0;

    elsif rising_edge(clk_100mhz) then
        sclk_reg1 <= sclk;
        sclk_reg2 <= sclk_reg1;

        cs_reg1 <= cs;
        cs_reg2 <= cs_reg1;

        fir_enable <= '0';

        if cs_reg1 = '1' and cs_reg2 = '0' then
            fir_enable <= '1';
        end if;

        if cs = '1' then
            shift_reg_tx <= fir_y_out;
            bit_count    <= 0;
        else
            if sclk_reg1 = '1' and sclk_reg2 = '0' then
                shift_reg_rx <= shift_reg_rx(14 downto 0) & mosi;
            end if;

            if sclk_reg1 = '0' and sclk_reg2 = '1' then
                shift_reg_tx <= shift_reg_tx(14 downto 0) & '0';
            end if;
        end if;

    end if;
end process;

end Behavioral;