function s = tntlib_markov_state(v,k)
%TNTLIB_MARKOV_STATE - Compute the markov state number
%
%   s = TNTLIB_MARKOV_STATE(v,k)
%
%   Input : v - (1,n)-vector holding the symbols in the memory (of size n).
%               The symbols must be in range 1 up to k.
%           k - Alphabet size
%   Output: s - Markov state (1 <= s <= k^n)

    % Attention: The k symbols must be in range 1 up to k!
    if min(v) < 1 || max(v) > k
        error('Vector v contains symbols out of range!');
    end
    
    [~,n] = size(v);
    v = v - 1; %< make range 0...k-1
    
    % Convert vector v to decimal state number s
    s = 0;
    for i = 1:n
        s = s + v(i)*k^(i-1);
    end
    
    s = s + 1;
end

