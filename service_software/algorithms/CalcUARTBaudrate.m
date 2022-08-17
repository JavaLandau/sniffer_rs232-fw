%> @file CalcBaudrate.m
%> @brief Get baudrate from defined list by bit's width
%> @param[in] Fmax - frequency of UART line scanning in Hz
%> @param[in] LenBit - length (width) of bit
%> @param[out] baudrate - numeric value of found baudrate (0 if not found)
%> @param[out] prescaler - prescaler for Fmax to scan line with speed
%> equaled to found baudrate (0 if baudrate is not found)
function [baudrate, prescaler] = CalcUARTBaudrate(Fmax, LenBit)

tolerance = 0.1;

baudrates = [921600, 460800, 230400, 115200, 57600, 38400, 19200, 9600, 4800, 2400];
calc_baud = round(Fmax / LenBit);
found_i = 0;

for i = 1:length(baudrates)
    baud = baudrates(i);
    if (calc_baud >= (1 - tolerance) * baud) && ((calc_baud <= (1 + tolerance) * baud))
        found_i = i;
        break;
    end
end

if found_i
    baudrate = baudrates(found_i);
    prescaler = round(Fmax / calc_baud);
else
    baudrate = 0;
    prescaler= 0;
end