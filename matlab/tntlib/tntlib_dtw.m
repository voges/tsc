function [p1,p2,C,D] = tntlib_dtw(v1,v2)
%TNTLIB_DTW - Dynamic Time Warping
%
%   [p1,p2,C,D] = TNTLIB_DTW(v1,v2)
%
%   Input : v1, v2 - Row vectors (1,n1), (1,n2) to be matched
%   Output: p1, p2 - Row vectors, containing optimal warping paths 
%                    through v1, v2
%           C      - Cost matrix
%           D      - Distance matrix

    n1 = size(v1,2);
    n2 = size(v2,2);
    
    %
    % Compute the cost matrix: C(i,j) = |v1(i)-v2(j)|
    %
    C = zeros(n1,n2);
    for row = 1:n1
       C(row,:) = abs(repmat(v1(row),1,n2)-v2);  
    end

    D = zeros(n1,n2);
    M = zeros(n1,n2);

    for col = 2:n2
        D(1,col) = D(1,col-1) + C(1,col);
        M(1,col) = 3;
    end
    
    for row = 2:n1
        D(row,1) = D(row-1,1) + C(row,1);
        M(row,1) = 2;
    end

    %
    % "A Maximum Likelihood Stereo Algorithm"
    %
    for i = 2:n1
        for j = 2:n2
            topLeft = D(i-1,j-1);
            top     = D(i-1,j) * 1.1; % penalty for occlusion: 10%
            left    = D(i,j-1) * 1.1; % penalty for occlusion: 10%
            [cmin,ind] = min([topLeft,top,left],[],2);
            D(i,j) = C(i,j) + cmin;
            M(i,j) = ind;
        end
    end
    
    %
    % Backtracking with M: Find the optimal path from C(1,1) to C(n1,n2)
    %
    r = n1;
    c = n2;
    p1 = zeros(1,max([n1,n2]));
    p1_itr = 1;
    p2 = zeros(1,max([n1,n2]));
    p2_itr = 1;
    
    while (r >= 1 && c >= 1)
        p1(p1_itr) = r; p1_itr = p1_itr + 1;
        p2(p2_itr) = c; p2_itr = p2_itr + 1;
        %p1 = [p1,r];
        %p2 = [p2,c];
        
        switch M(r,c)
            case 1
                r = r-1;
                c = c-1;
            case 2
                r = r-1;
            case 3
                c = c-1;
            otherwise
                % M(1,1) = 0, break the loop
                r = r-1;
                c = c-1;
        end
    end
    
    % Flip the paths such that they start at (1,1)
    p1 = fliplr(p1);
    p2 = fliplr(p2);
end

