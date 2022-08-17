%> @file GenerateUARTLine.m
%> @brief Generator of UART line (random packets + random duration of idle
%> states). Frequency of generator = <Fmax> * <baudrate> (<Fmax> is
%> frequency of line's scanning, determined as input parameter in @file
%> SnifferRS232.m.
%>
%> @param[in] UARTParams - struct contains UART parameters: bits count,
%> stop bit, parity, baudrate (as frequency of generator)
%> @param[in] Init - flag whether it's needed to initialize internal state
%> of generator, used before first call of the function 
%> @param[out] Bit - return state of UART line (0 - dominant state, 1 -
%> recessive state)
function Bit = GenerateUARTLine(UARTParams, Init)
persistent UARTState
persistent PauseCnt

LenPause = 15;

if Init
    UARTState = 'pause';
    PauseCnt = round(LenPause*rand) + 8*UARTParams.baudrate;
    GenerateUARTPacket(UARTParams, true);
    Bit = 0;
elseif strcmp(UARTState,'pause')
    PauseCnt = PauseCnt - 1;
    if(~PauseCnt)
        UARTState = 'packet';
    end
    Bit = 1;
else
    [Bit,status] = GenerateUARTPacket(UARTParams, false);
    
    if(strcmp(status,'end'))
        PauseCnt = round(LenPause*rand) + 8*UARTParams.baudrate;
        UARTState = 'pause';
    end
end
