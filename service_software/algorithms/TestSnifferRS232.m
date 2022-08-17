%> @file TestSnifferRS232.m
%> @brief Stand to test algorithm determined in @file SnifferRS232.m
%>
%> @param[in] Fmax - frequency of UART line scanning in Hz (should be at
%> least four times faster than @ref UARTBaudrate
%> @param[in] UARTBaudrate - baudrate of scanned UART line
%> @param[in] BitsCount - bits count of scanned UART line
%> @param[in] Parity - type of parity of scanned UART line
%> @param[out] Statistic - struct contained result of the test: count of
%> valid and invalid algorithm's output data
function Statistic = TestSnifferRS232(Fmax, UARTBaudrate, BitsCount, Parity, CountTest)

Statistic = struct('total',0,'valid',0,'invalid',0);

for i = 1:CountTest
    [baudrate,hypothesis] = SnifferRS232(Fmax, UARTBaudrate, BitsCount, Parity);
    
    if(baudrate ~= UARTBaudrate || hypothesis.bits_count ~= BitsCount || ...
       ~strcmp(hypothesis.parity, Parity))
        Statistic.invalid = Statistic.invalid + 1;
    else
        Statistic.valid = Statistic.valid + 1;
    end
    fprintf('Process = %%%d\n',round(100*i/CountTest));
end

