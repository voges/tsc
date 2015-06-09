function status = qual_markovfir(fn)
%QUAL_MARKOVFIR Code quality scores using an FIR filter
%
%   status = QUAL_MARKOVFIR(fn)
%
%   Input : fn     - File name
%   Output: status - Returns 1 on success, otherwise 0

    block_sz = 1000;               %< number of lines per block
    block_n = 10;                   %< number of blocks
    fid = fopen([fn,'.qual'],'r');  %< open file
    
    % Entropies
    q_entropy = 0;
    c_entropy = 0;

    for b = 1:block_n
        fprintf('Encoding block %d/%d ...\n',b,block_n);
        Qual = tntlib_read_ascii(fid,block_sz);

        % Write the quality score lines to a matrix Q. The lines might have
        % different lengths. Short lines are being filled with trailing NaN's.
        l = zeros(block_sz,1);
        [max_line_length,~] = max(cellfun(@numel,Qual));
        Q = zeros(block_sz,max_line_length) .* nan;
        for i = 1:block_sz
            l(i) = length(Qual{i,1});
            Q(i,1:l(i)) = Qual{i,1};
        end

        % Q contains k different symbols s in some range
        %    0 <= min(min(M)) <= s <= max(max(M))
        % Map the symbols to the interval 0 <= s <= k-1:
        %  - Make 0 the smallest symbol
        %  - Find empty symbol slots in range 1 <= s <= k-1 and fill them
        %    with symbols s > k
        k = length(unique(Q(~isnan(Q)))); %< number of different symbols
        Q = Q - min(min(Q));
        for i = 1:k-1
            if isempty(Q(Q == i))
                Q(Q == max(max(Q))) = i;
            end
        end

        % Entropy of raw quality score stream
        q_entropy = q_entropy + tntlib_entropy(Q);
        
        N = 30;
        E = ones(size(Q,1),size(Q,2)) .* NaN;
        line_prev = Q(1,:);
        line_prev = line_prev(~isnan(line_prev));
        line_len = length(line_prev);
        for r = 2:block_sz
            line_prev = Q(r-1,:);
            line_prev = line_prev(~isnan(line_prev));
            line_curr = Q(r,:);
            line_curr = line_curr(~isnan(line_curr));

            % Strip trailing sequence of score '1'
            while length(line_curr) > 1 && line_curr(end) == 1
                line_curr = line_curr(1:end - 1);
            end
            line_len = [line_len;length(line_curr)];

            if (line_len(r) >= N)
                a = lpc(line_prev,N);
                line_est = round(filter([0 -a(2:end)],1,line_curr));
                E(r,1:line_len(r)) = line_curr-line_est;
            end
         
        end

        
        figure(1);
        [x,h] = tntlib_integer_histogram(Q(~isnan(Q)));
        subplot(2,3,1); imagesc(Q);
        subplot(2,3,4); bar(x,h);
        [x,h] = tntlib_integer_histogram(E(~isnan(E)));
        subplot(2,3,2); imagesc(E);
        subplot(2,3,5); bar(x,h);

        % FIR filter column-wise
        N = 3;
        EE = ones(size(Q,1),size(Q,2)) .* NaN;
        for c = 1:max_line_length
            col = E(:,c);
            col = col(~isnan(col));

            a = lpc(col,N);
            col_est = round(filter([0 -a(2:end)],1,col));
            EE(1:length(col),c) = col-col_est;

        end
        
        [x,h] = tntlib_integer_histogram(EE(~isnan(EE)));
        subplot(2,3,3); imagesc(EE);
        subplot(2,3,6); bar(x,h);
        
        fprintf('Q: %f\n', tntlib_entropy(Q));
        fprintf('E: %f\n', tntlib_entropy(E));
        fprintf('EE: %f\n', tntlib_entropy(EE));
    end
    
    fclose(fid);
    status = 1;
end

