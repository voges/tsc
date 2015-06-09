function status = qual_markovpredictorcomplex(fn)
%QUAL_MARKOVPREDICTORCOMPLEX Reducing the entropy employing complex Markov model
%
%   status = QUAL_MARKOVPREDICTORCOMPLEX(fn)
%
%   Input : fn     - File name
%   Output: status - Returns 1 on success, otherwise 0

    block_sz = 100;                 %< number of lines per block
    block_n = 10;                   %< number of blocks
    fid = fopen([fn,'.qual'],'r');  %< open file
    
    % Entropies
    Q_entropy = 0;
    C_entropy = 0;

    % Markov model parameters
    m_sz = 2;                  %< memory size
    m_x_off = 2;
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

        % Q contains k different symbols s in some range
        %    0 <= min(Q) <= s <= max(Q)
        % Map the symbols to the interval 1 <= s <= k:
        %  - Make 1 the smallest symbol
        %  - Find empty symbol slots in range 2 <= s <= k and fill them
        %    with symbols s > k
        Q = Q - min(min(Q)) + 1;
        for i = 2:k
            if isempty(Q(Q == i))
                Q(Q == max(max(Q))) = i;
            end
        end
        Q_entropy = Q_entropy + tntlib_entropy(Q(~isnan(Q)));

        % Encoding
        q_ctx_sz = 100;
        q_ctx = zeros(q_ctx_sz,max_line_length) .* nan;
        q_id = zeros(q_ctx_sz,4);
        q_id_weights = [0.25;0.25;0.25;0.25];
        C = zeros(size(Q)) .* nan;
        
        % Write first line to encoded stream and to line context
        q = Q(1,:);
        q = q(~isnan(q));
        C(1,1:length(q)) = q;
        q_ctx(1,1:length(q)) = q;
        q_id(1,:) = [length(q),mean(q),var(q),0];
        q_ctx_n = 1;
        
        for line = 2:block_sz
            % Get current line
            q = Q(line,:);
            q = q(~isnan(q));
            q_len = length(q);
            
            % Add line to q_ctx and to q_id
            q_ctx = circshift(q_ctx,1);
            q_ctx(1,:) = q_ctx(1,:) .* nan;
            q_ctx(1,1:q_len) = q;
            q_id = circshift(q_id,1);
            q_id(1,:) = [q_len,mean(q),var(q),0];
            
            if q_ctx_n < q_ctx_sz
                q_ctx_n = q_ctx_n + 1; % count fill status of q_ctx
            end
            
            % Find best matching line in line context
            diff = abs((repmat(q_id(1,:),q_ctx_n-1,1) - q_id(2:q_ctx_n,:))) * q_id_weights;
            [~,ind] = min(diff);
            q_ctx_match = ind + 1;
            q_match = q_ctx(q_ctx_match,:);
            
            % Encode with Markov memory
            C(line,1:m_x_off) = q(1:m_x_off);
            for ind = m_x_off+1:q_len
                % Get memory symbols
                if (ind <= length(q_match))
                    m_inter = q_match(ind);
                    m_intra = q(ind-1);%q(ind-m_x_off:ind-1);
                    
                else
                    m_inter = [];
                    m_intra = q(ind-m_sz:ind-1);
                end
                m = [m_inter,m_intra];
                
                % Encode prediction error
                s = tntlib_markov_state(m,k);
                C(line,ind) = q(ind) - t(s);
                
                % Update prediction table
                P(s,q(ind)) = P(s,q(ind)) + 1;
                [~,t_new] = max(P,[],2);
                t(s) = t_new(s);
            end
        end
        C_entropy = C_entropy + tntlib_entropy(C);
    end
    
    fprintf('Q: %.4f bit/symbol\n',Q_entropy/block_n);
    fprintf('C: %.4f bit/symbol\n',C_entropy/block_n);
    figure(1);
    subplot(1,2,1); imagesc(Q);
    subplot(1,2,2); imagesc(C);

    fclose(fid);
    status = 1;
end


