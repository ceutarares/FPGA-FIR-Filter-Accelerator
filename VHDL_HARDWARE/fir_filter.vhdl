library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use ieee.std_logic_arith.all;
use ieee.std_logic_signed.all;

entity fir_filter is
generic (
    w1: integer := 16; 
    w2: integer := 32; 
    w3: integer := 36; 
    w4: integer := 16; 
    L : integer := 64  
);
Port ( 
    clk    : in std_logic;
    reset  : in std_logic;
    load_x : in std_logic;
    enable : in std_logic;
    x_in   : in std_logic_vector(w1-1 downto 0); 
    c_in   : in std_logic_vector(w1-1 downto 0);
    y_out  : out std_logic_vector(w4-1 downto 0)
);
end fir_filter;

architecture fpga of fir_filter is

subtype slv_w1 is std_logic_vector(w1-1 downto 0);
subtype slv_w2 is std_logic_vector(w2-1 downto 0);
subtype slv_w3 is std_logic_vector(w3-1 downto 0);

type array_slv_w1 is array (0 to L-1) of slv_w1;
type array_slv_w2 is array (0 to L-1) of slv_w2;
type array_slv_w3 is array (0 to L-1) of slv_w3;

signal coef_reg  : array_slv_w1;
signal products  : array_slv_w2;
signal acc       : array_slv_w3;

begin

multipliers: for i in 0 to L-1 generate
    products(i) <= coef_reg(i) * x_in;
end generate;

main_process: process(clk, reset)
begin
    if reset = '1' then
        for k in 0 to L-1 loop
            coef_reg(k) <= (others => '0');
            acc(k)      <= (others => '0');
        end loop;

    elsif rising_edge(clk) then
        if enable = '1' then
            if load_x = '0' then
                coef_reg(L-1) <= c_in;
                for i in L-2 downto 0 loop
                    coef_reg(i) <= coef_reg(i+1);
                end loop;
            else
                for i in 0 to L-2 loop
                    acc(i) <= SXT(products(i), w3) + acc(i+1);
                end loop;
                acc(L-1) <= SXT(products(L-1), w3);
            end if;
        end if;
    end if;
end process;

y_out <= acc(0)(w3-1 downto w3-w4);

end fpga;