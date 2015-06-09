function h = tntlib_entropy(S)
%TNTLIB_ENTROPY - Compute the entropy h of a given sequence S
% 
%   h = TNTLIB_ENTROPY(S)
%
%   Input : S - Sequence as (m,n)-matrix containing integers
%   Output: h - Entropy of the sequence in bit/symbol
    
    % Determine the symbol value range in the sequence
    S_max = max(max(S));
    S_min = min(min(S));
    k = S_max - S_min + 1;

    % Vector holding the frequencies of the symbols
    p = zeros(k,1);

    for i = 1:k
       p(i) = length(S(S == (S_min + i - 1)));
    end
    
    p = p ./ sum(p);
    p = p(p > 0); %< log2(0) would yield -Inf
    h = -sum(p .* log2(p));
end

