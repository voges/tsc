function status = qual_markovpredictorsimple(fn)
%QUAL_MARKOVPREDICTORSIMPLE Reducing the entropy employing a simple Markov model
%
%   status = QUAL_MARKOVPREDICTORSIMPLE(fn)
%
%   Input : fn     - File name
%   Output: status - Returns 1 on success, otherwise 0

    block_sz = 100;               %< number of lines per block
    block_n = 10;                   %< number of blocks
    fid = fopen([fn,'.qual'],'r');  %< open file
    
    % Entropies
    q_entropy = 0;
    c_entropy = 0;

    % Markov model parameters
    m_sz = 2;                  %< memory size
    k = 50;                    %< number of different symbols
    n = k^m_sz;                %< number of states
    
    % Initialize Markov model and prediction table
    t = repmat(1:k,1,n/k)';    %< initialize the prediction table
                               %  with values between 1 and k
    P = zeros(n,k);            %< Symbol occurences are counted here
                               %  to update the prediction table t
        
    fprintf('Encoding ...\n');
    for b = 1:block_n
        fprintf('Encoding block %d/%d ...\n',b,block_n);
        Qual = tntlib_read_ascii(fid,block_sz);

        % Write the quality score lines to a matrix Q. The lines might have
        % different lengths. Short lines are being filled with trailing NaN's.
        [max_line_length,~] = max(cellfun(@numel,Qual));
        Q = zeros(block_sz,max_line_length) .* nan;
        l = zeros(block_sz,1);
        for i = 1:block_sz
            l(i) = length(Qual{i,1});
            Q(i,1:l(i)) = Qual{i,1};
        end

        % Write the quality scores row-wise to a stream q, remove NaNs.
        q = reshape(Q',1,size(Q,1) * size(Q,2));
        q = q(~isnan(q));

        % q contains k different symbols s in some range
        %    0 <= min(q) <= s <= max(q)
        % Map the symbols to the interval 1 <= s <= k:
        %  - Make 1 the smallest symbol
        %  - Find empty symbol slots in range 2 <= s <= k and fill them
        %    with symbols s > k
        q = q - min(min(q)) + 1;
        for i = 2:k
            if isempty(q(q == i))
                q(q == max(max(q))) = i;
            end
        end
        
        % Entropy of raw quality score stream
        q_entropy = q_entropy + tntlib_entropy(q);

        % Encoding
        c = zeros(1,length(q)); %< stream with encoded symbols
        c(1:m_sz) = q(1:m_sz);
        for ind = m_sz+1:length(q)
            % Get memory symbols
            m = q(ind-m_sz:ind-1);
            
            % Encode prediction error
            s = tntlib_markov_state(m,k);
            c(ind) = q(ind) - t(s);
            
            % Update prediction table
            P(s,q(ind)) = P(s,q(ind)) + 1;
            [~,t_new] = max(P,[],2);
            t(s) = t_new(s);
        end
        
        % Entropy of coded stream
        c_entropy = c_entropy + tntlib_entropy(c);
        
        % Write coded stream column-wise
        C = zeros(size(Q)) .* nan;
        c_ind = 1;
        for line = 1:block_sz
            C(line,1:l(line)) = c(c_ind:c_ind + l(line) - 1);
            c_ind = c_ind + l(line);
        end
        
        % Write the quality scores colum-wise to a stream q, remove NaNs.
        q = reshape(C,1,size(C,1) * size(C,2));
        q = q(~isnan(q));
        
        subplot(1,4,1); imagesc(C);
        
        [x,h] = tntlib_integer_histogram(q);
        subplot(1,4,2); bar(x,h);
        title('Histogram');
        xlabel('Symbol value');
        ylabel('Absolute frequency');

        [acorr,lags] = xcorr(q,'coeff');
        subplot(1,4,3); plot(lags,acorr); grid;
        title('Column-wise Autocorrelation');
        xlabel('Lags');
        ylabel('Normalized value');

        [acorr,lags] = xcorr(c,'coeff');
        subplot(1,4,4); plot(lags,acorr); grid;
        title('Row-wise Autocorrelation');
        xlabel('Lags');
        ylabel('Normalized value');
            
    end
    
    fprintf('q: %.4f bit/symbol\n',q_entropy/block_n);
    fprintf('c: %.4f bit/symbol\n',c_entropy/block_n);

    fclose(fid);
    status = 1;
end

