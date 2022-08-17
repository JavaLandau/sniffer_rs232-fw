%> @file GenerateUARTPacket.m
%> @brief Generator of random UART packets. Part of the function determined
%> in @file GenerateUARTLine.m and inherits its settings via input
%> parameter UARTParams.
%>
%> @param[in] UARTParams - struct contains UART parameters: bits count,
%> stop bit, parity, baudrate (as frequency of generator)
%> @param[in] Init - flag whether it's needed to initialize internal state
%> of generator, used before first call of the function 
%> @param[out] Bit - return current bit of UART packet
function [bit,status] = GenerateUARTPacket(UARTParams, Init)
persistent internal_cnt
persistent current_bit
persistent CurPacket
persistent packet_state
persistent CorrCoeff

if isempty(packet_state) || Init
    packet_state = 'startbit';
end

if isempty(CurPacket) || Init
    CurPacket = zeros(1,9);
end

if isempty(current_bit) || Init
    current_bit = 1;
end

if isempty(CorrCoeff) || Init
    CorrCoeff = 1;
end

if isempty(internal_cnt) || Init
    internal_cnt = 0;
else
    internal_cnt = internal_cnt + 1;
end

if Init
    status = '';
    bit = 0;
    return;
end

status = 'process';
if(internal_cnt == round((UARTParams.baudrate)/CorrCoeff))
    internal_cnt = 0;
    switch(packet_state)
        case 'startbit'
            current_bit = 0;
            packet_state = 1;
        case {1,2,3,4,5,6,7,8,9}
            current_bit = round(rand);
            CurPacket(packet_state) = current_bit;            
            if(UARTParams.bits_count == packet_state)
                if(~strcmp(UARTParams.parity,'none'))
                    packet_state = 'parity';
                else
                    packet_state = 'stopbit_1';
                end
            else
                packet_state = packet_state + 1;
            end
        case 'parity'    
            if(strcmp(UARTParams.parity,'even'))
                current_bit = rem(sum(CurPacket(1:UARTParams.bits_count)),2);
            else
                current_bit = 1 - rem(sum(CurPacket(1:UARTParams.bits_count)),2);
            end
            packet_state = 'stopbit_1';
        case 'stopbit_1'
            current_bit = 1;
            switch(UARTParams.stop_bits)
                case 'stopbit_1'
                    packet_state = 'startbit';
                    status = 'end';
                case 'stop_bit_1.5'
                    packet_state = 'stop_bit_1.5';
                    CorrCoeff = 2;
                case 'stopbit_2'
                    packet_state = 'stop_bit_2';
            end
        case 'stop_bit_1.5'
            packet_state = 'startbit';
            status = 'end';
            CorrCoeff = 1;
        case 'stop_bit_2'
            packet_state = 'startbit';
            status = 'end';
    end
end

bit = current_bit;