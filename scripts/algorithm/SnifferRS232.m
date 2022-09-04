%> @file SnifferRS232.m
%> @brief Idenitifes parameters of UART line: baudrate, bit count, parity.
%> Fast GPIO scanning is used to achieve it.
%> NOTE. Algorithm will not work correctly if UART line does not contain
%> idle states. If so algotihm will be able to identify baudrate only.
%>
%> @param[in] Fmax - frequency of UART line scanning in Hz (should be at
%> least four times faster than @ref UARTBaudrate
%> @param[in] UARTBaudrate - baudrate of scanned UART line
%> @param[in] BitsCount - bits count of scanned UART line
%> @param[in] Parity - type of parity of scanned UART line
%> @param[out] baudrate - found baudrate of scanned UART line
%> @param[out] hypothesis - struct contained found parameters of UART line:
%> bit count, parity.
function [baudrate, hypothesis] = SnifferRS232(Fmax, UARTBaudrate, BitsCount, Parity)

IdleCount = 12;
MaxValidPacket = 20;

baudrate = round(Fmax / UARTBaudrate);
UARTParams = struct('baudrate',baudrate,'bits_count',BitsCount,'parity',Parity,'stop_bits','stopbit_1');

%Sequence of hypothesises by scheme: first all hypothesises with total bit
%count equaled to 7, then all with count equaled to 8, then - equalted to 9
%and finally 10. Every time when total bit count is not correct (due to
%reccesive stop bit, next hypothesis number is equaled to field 'jump'
%where first hyphothesis with new total bit count is located.
%NOTE. Every time when baudrate is recalculated hypothesis number is
%resetted to 1
hyp_seq = [ struct('bits_count',7,'parity','none', 'count', 7, 'jump', 2),...
            struct('bits_count',7,'parity','even', 'count', 8, 'jump', 5),...
            struct('bits_count',7,'parity','odd',  'count', 8, 'jump', 5),...
            struct('bits_count',8,'parity','none', 'count', 8, 'jump', 5),...
            struct('bits_count',8,'parity','even', 'count', 9, 'jump', 8),...
            struct('bits_count',8,'parity','odd', 'count', 9, 'jump', 8),...
            struct('bits_count',9,'parity','none', 'count', 9, 'jump', 8),...
            struct('bits_count',9,'parity','even', 'count', 10, 'jump', 1),...
            struct('bits_count',9,'parity','odd', 'count', 10, 'jump', 1)];

hyp_num = 1;
valid_packet = 0;


LenBit = 0;
MinLenBit = Inf;
idle_cnt = 0;
startbit_cnt = 0;
pkt_cnt = 0;
pkt_tick_cnt = 0;
rec_state = 'idle';
parity_bit = 0;
parity_calc = 0;
prescaler = 0;
baudrate = 0;

GenerateUARTLine(UARTParams,true);

while(true)    
    Bit = GenerateUARTLine(UARTParams,false);

    %Block of baudrate's calculation
    if(Bit == 0)
        LenBit = LenBit + 1;
    elseif(LenBit ~= 0)
        if(LenBit < MinLenBit)
            MinLenBit = LenBit;
            [new_baudrate, prescaler] = CalcUARTBaudrate(Fmax, MinLenBit);
                     
            if baudrate ~= new_baudrate && new_baudrate
                baudrate = new_baudrate;
                rec_state = 'idle';
                idle_cnt = 0;
                pkt_cnt = 0;
                pkt_tick_cnt = 0;
                startbit_cnt = 0;
                parity_calc = 0;
                hyp_num = 1;

                disp(['[SNIFFER]: found baudrate ',num2str(baudrate)]);
                disp('[SNIFFER]: hypothesis resets to 1 due to baudrate');
                valid_packet = 0;
            elseif ~new_baudrate
                disp('[SNIFFER]: invalid minimal bit pulse');
                MinLenBit = Inf;
            end
        end
        LenBit = 0;
    end 

    %Block to monitor idle state of UART line
    %Once idle state is found the algorithm will wait start bit
    if prescaler && Bit
        idle_cnt = idle_cnt + 1;

        if idle_cnt == IdleCount*prescaler
            rec_state = 'startbit';
            idle_cnt = 0;
            pkt_cnt = 0;
            pkt_tick_cnt = 0;
            startbit_cnt = 0;
            parity_calc = 0;
        end
    else
        idle_cnt = 0;
    end

    switch(rec_state)
        %Wait for startbit and if found positions to middle of bit 
        case 'startbit'
            if Bit == 0
                startbit_cnt = startbit_cnt + 1;

                if startbit_cnt == round(prescaler/2)
                    rec_state = 'packet';
                    startbit_cnt = 0;
                    pkt_tick_cnt = 0;
                    pkt_cnt = 0;
                    parity_calc = 0;
                end
            end
        %Read all bits of UART packet: assumed total bit count + 1 stop bit
        case 'packet'
            pkt_tick_cnt = pkt_tick_cnt + 1;

            if pkt_tick_cnt == prescaler
                pkt_tick_cnt = 0;
                pkt_cnt = pkt_cnt + 1;

                if pkt_cnt < hyp_seq(hyp_num).count
                    parity_calc = xor(parity_calc, Bit);
                elseif pkt_cnt == hyp_seq(hyp_num).count
                    parity_bit = Bit;
                elseif pkt_cnt == (hyp_seq(hyp_num).count + 1)
                    if Bit == 0
                        hyp_num_ = hyp_num;
                        hyp_num = hyp_seq(hyp_num).jump;
                        valid_packet = 0;
                        disp(['[SNIFFER]: jump hypothesis ',num2str(hyp_num_),'->',num2str(hyp_num)]);
                    else
                        status = true;

                        if strcmp(hyp_seq(hyp_num).parity,'even')
                            status = parity_bit == parity_calc;
                        elseif strcmp(hyp_seq(hyp_num).parity,'odd')
                            status = parity_bit == (1 - parity_calc);
                        end

                        if ~status
                            valid_packet = 0;
                            hyp_num  = hyp_num + 1;

                            if hyp_num > length(hyp_seq)
                                hyp_num = 1;
                                disp('[SNIFFER]: wrap arounded hypothesis to 1');
                            else
                                disp(['[SNIFFER]: change hypothesis ',num2str(hyp_num - 1),'->',num2str(hyp_num)]);
                            end
                        else
                            valid_packet = valid_packet + 1;
                        end
                    end
                    pkt_cnt = 0;
                    parity_calc = 0;
                    rec_state = 'idle';
                end
            end                
    end

    %If hypothesis is stable within @var MaxValidPacket it will assume to
    %be correct and terminate the algorithm
    if valid_packet == MaxValidPacket
        disp(['[SNIFFER]: baudrate = ',num2str(baudrate),'; bit_count = ',...
                num2str(hyp_seq(hyp_num).bits_count),'; parity = ',...
                hyp_seq(hyp_num).parity]);
        break;
    end
end

hypothesis = hyp_seq(hyp_num);